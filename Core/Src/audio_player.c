/**
 * @file audio_player.c
 * @brief Audio player implementation for PCM files via I2S/SmartPA
 * @version 1.0.0
 * @date 2025-12-09
 */

#include "audio_player.h"
#include "stm32_aw882xx_adapter.h"
#include "i2s.h"
#include "ff.h"
#include "shell_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

/********************************************
 * Log module definition
 *******************************************/
#define SHELL_LOG_AUDIO_DEBUG(fmt, ...)   SHELL_LOG_DEBUG(SHELL_LOG_MODULE_AUDIO, fmt, ##__VA_ARGS__)
#define SHELL_LOG_AUDIO_INFO(fmt, ...)    SHELL_LOG_INFO(SHELL_LOG_MODULE_AUDIO, fmt, ##__VA_ARGS__)
#define SHELL_LOG_AUDIO_WARNING(fmt, ...) SHELL_LOG_WARNING(SHELL_LOG_MODULE_AUDIO, fmt, ##__VA_ARGS__)
#define SHELL_LOG_AUDIO_ERROR(fmt, ...)   SHELL_LOG_ERROR(SHELL_LOG_MODULE_AUDIO, fmt, ##__VA_ARGS__)

/********************************************
 * External HAL handles
 *******************************************/
extern I2S_HandleTypeDef hi2s2;

/********************************************
 * Private variables
 *******************************************/

/* Double buffer for I2S DMA - in D2 SRAM for DMA1/DMA2 access */
static uint32_t g_audio_buffer[AUDIO_BUFFER_SIZE * 2] __attribute__((aligned(32), section(".RAM_D2")));

/* Dummy RX buffer for full-duplex I2S (required even if not used) */
static uint32_t g_audio_rx_buffer[AUDIO_BUFFER_SIZE * 2] __attribute__((aligned(32), section(".RAM_D2")));

/* Intermediate buffer for file reading - in D1 SRAM for SDMMC IDMA access */
/* SDMMC IDMA can ONLY access AXI SRAM (D1 domain), NOT D2 SRAM */
#define FILE_READ_CHUNK_SIZE  4096
static uint8_t g_file_read_buffer[FILE_READ_CHUNK_SIZE] __attribute__((aligned(32)));

/* File handle */
static FIL g_audio_file;
static bool g_file_opened = false;

/* Player state */
static volatile AudioState_t g_player_state = AUDIO_STATE_IDLE;
static volatile bool g_loop_enabled = false;
static volatile uint32_t g_total_bytes = 0;
static volatile uint32_t g_played_bytes = 0;
static volatile uint32_t g_file_position = 0;

/* Audio format */
static AudioFormat_t g_audio_format = {
    .sample_rate = 48000,
    .bits_per_sample = 32,
    .channels = 2,
};

/* Synchronization - use message queue for reliable ISR to task communication */
static QueueHandle_t g_buffer_queue = NULL;
#define BUFFER_QUEUE_LENGTH  32  /* Increased to handle burst of DMA callbacks */

/* Message type for buffer fill request */
typedef struct {
    uint8_t buffer_index;  /* 0 = first half, 1 = second half */
} BufferFillMsg_t;

/* Debug counters */
static volatile uint32_t g_dma_half_count = 0;
static volatile uint32_t g_dma_full_count = 0;
static volatile uint32_t g_task_fill_count = 0;  /* Track successful buffer fills */
static volatile uint32_t g_task_timeout_count = 0;  /* Track queue receive timeouts */
static volatile uint32_t g_file_read_count = 0;   /* Track f_read calls */
static volatile uint32_t g_memcpy_count = 0;      /* Track memcpy calls */
static volatile uint32_t g_queue_send_count = 0;  /* Track queue send from ISR */
static volatile uint32_t g_queue_send_fail = 0;   /* Track queue send failures */

/* Player task */
static TaskHandle_t g_player_task_handle = NULL;
#define PLAYER_TASK_STACK_SIZE  2048  /* Increased for file I/O operations */
#define PLAYER_TASK_PRIORITY    (configMAX_PRIORITIES - 2)  /* High priority for real-time audio */

/********************************************
 * Private function prototypes
 *******************************************/
static void audio_player_task(void *pvParameters);
static int fill_buffer(uint32_t *buffer, uint32_t samples);
static int start_dma_playback(void);
static void stop_dma_playback(void);

/********************************************
 * Public functions
 *******************************************/

int audio_player_init(void)
{
    SHELL_LOG_AUDIO_INFO("Initializing audio player...");
    
    /* Create message queue for buffer fill requests */
    if (g_buffer_queue == NULL) {
        g_buffer_queue = xQueueCreate(BUFFER_QUEUE_LENGTH, sizeof(BufferFillMsg_t));
        if (g_buffer_queue == NULL) {
            SHELL_LOG_AUDIO_ERROR("Failed to create buffer queue");
            return -1;
        }
    }
    
    g_player_state = AUDIO_STATE_IDLE;
    g_file_opened = false;
    
    SHELL_LOG_AUDIO_INFO("Audio player initialized");
    return 0;
}

void audio_player_deinit(void)
{
    audio_player_stop();
    
    if (g_buffer_queue != NULL) {
        vQueueDelete(g_buffer_queue);
        g_buffer_queue = NULL;
    }
    
    SHELL_LOG_AUDIO_INFO("Audio player deinitialized");
}

int audio_player_play_file(const char *filepath, AudioFormat_t *format, bool loop)
{
    FRESULT fres;
    
    if (filepath == NULL) {
        SHELL_LOG_AUDIO_ERROR("Invalid filepath");
        return -1;
    }
    
    /* Stop any existing playback */
    if (g_player_state == AUDIO_STATE_PLAYING) {
        audio_player_stop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    SHELL_LOG_AUDIO_INFO("Opening file: %s", filepath);
    
    /* Open the PCM file */
    fres = f_open(&g_audio_file, filepath, FA_READ);
    if (fres != FR_OK) {
        SHELL_LOG_AUDIO_ERROR("Failed to open file: %s (err=%d)", filepath, fres);
        return -2;
    }
    g_file_opened = true;
    
    /* Get file size */
    g_total_bytes = f_size(&g_audio_file);
    g_played_bytes = 0;
    g_file_position = 0;
    
    SHELL_LOG_AUDIO_INFO("File size: %lu bytes", g_total_bytes);
    
    /* Set audio format */
    if (format != NULL) {
        memcpy(&g_audio_format, format, sizeof(AudioFormat_t));
    }
    
    g_loop_enabled = loop;
    
    /* Calculate duration */
    uint32_t bytes_per_sample = (g_audio_format.bits_per_sample / 8) * g_audio_format.channels;
    uint32_t total_samples = g_total_bytes / bytes_per_sample;
    uint32_t duration_ms = (total_samples * 1000) / g_audio_format.sample_rate;
    
    SHELL_LOG_AUDIO_INFO("Format: %luHz, %dbit, %dch", 
                         g_audio_format.sample_rate,
                         g_audio_format.bits_per_sample,
                         g_audio_format.channels);
    SHELL_LOG_AUDIO_INFO("Duration: %lu ms, Loop: %s", duration_ms, loop ? "ON" : "OFF");
    
    /* Pre-fill both buffers FIRST */
    SHELL_LOG_AUDIO_DEBUG("Pre-filling buffer 0...");
    int fill_ret = fill_buffer(&g_audio_buffer[0], AUDIO_BUFFER_SAMPLES);
    SHELL_LOG_AUDIO_DEBUG("Buffer 0 filled, ret=%d", fill_ret);
    
    SHELL_LOG_AUDIO_DEBUG("Pre-filling buffer 1...");
    fill_ret = fill_buffer(&g_audio_buffer[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES);
    SHELL_LOG_AUDIO_DEBUG("Buffer 1 filled, ret=%d", fill_ret);
    
    SHELL_LOG_AUDIO_DEBUG("Creating player task...");
    
    /* Create player task */
    if (g_player_task_handle == NULL) {
        BaseType_t xReturned = xTaskCreate(
            audio_player_task,
            "AudioPlayer",
            PLAYER_TASK_STACK_SIZE,
            NULL,
            PLAYER_TASK_PRIORITY,
            &g_player_task_handle
        );
        
        if (xReturned != pdPASS) {
            SHELL_LOG_AUDIO_ERROR("Failed to create player task");
            f_close(&g_audio_file);
            g_file_opened = false;
            return -4;
        }
        SHELL_LOG_AUDIO_DEBUG("Player task created OK");
    }
    
    SHELL_LOG_AUDIO_DEBUG("Calling start_dma_playback...");
    
    /* Set state to PLAYING BEFORE starting DMA so callbacks can work */
    g_player_state = AUDIO_STATE_PLAYING;
    
    /* Start DMA playback BEFORE SmartPA - I2S clock must be running for PLL lock */
    int ret = start_dma_playback();
    if (ret != 0) {
        SHELL_LOG_AUDIO_ERROR("Failed to start DMA playback: %d", ret);
        g_player_state = AUDIO_STATE_IDLE;
        f_close(&g_audio_file);
        g_file_opened = false;
        return -5;
    }
    
    /* Wait for I2S clock to stabilize */
    SHELL_LOG_AUDIO_DEBUG("Waiting for I2S clock to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Now start SmartPA - PLL should be able to lock to I2S clock */
    ret = aw882xx_adapter_start();
    if (ret != 0) {
        SHELL_LOG_AUDIO_WARNING("SmartPA start returned: %d (may still work)", ret);
        /* Don't fail here - SmartPA might still work even with warning */
    }
    
    SHELL_LOG_AUDIO_INFO("Playback started");
    
    return 0;
}

int audio_player_stop(void)
{
    if (g_player_state == AUDIO_STATE_IDLE) {
        return 0;
    }
    
    SHELL_LOG_AUDIO_INFO("Stopping playback...");
    
    g_player_state = AUDIO_STATE_STOPPED;
    
    /* Stop DMA */
    stop_dma_playback();
    
    /* Stop SmartPA */
    aw882xx_adapter_stop();
    
    /* Close file */
    if (g_file_opened) {
        f_close(&g_audio_file);
        g_file_opened = false;
    }
    
    /* Delete player task */
    if (g_player_task_handle != NULL) {
        vTaskDelete(g_player_task_handle);
        g_player_task_handle = NULL;
    }
    
    g_player_state = AUDIO_STATE_IDLE;
    SHELL_LOG_AUDIO_INFO("Playback stopped");
    
    return 0;
}

int audio_player_pause(void)
{
    if (g_player_state != AUDIO_STATE_PLAYING) {
        return -1;
    }
    
    HAL_I2S_DMAPause(&hi2s2);
    g_player_state = AUDIO_STATE_PAUSED;
    SHELL_LOG_AUDIO_INFO("Playback paused");
    
    return 0;
}

int audio_player_resume(void)
{
    if (g_player_state != AUDIO_STATE_PAUSED) {
        return -1;
    }
    
    HAL_I2S_DMAResume(&hi2s2);
    g_player_state = AUDIO_STATE_PLAYING;
    SHELL_LOG_AUDIO_INFO("Playback resumed");
    
    return 0;
}

int audio_player_get_status(AudioPlayerStatus_t *status)
{
    if (status == NULL) {
        return -1;
    }
    
    uint32_t bytes_per_sample = (g_audio_format.bits_per_sample / 8) * g_audio_format.channels;
    
    status->state = g_player_state;
    status->total_samples = g_total_bytes / bytes_per_sample;
    status->played_samples = g_played_bytes / bytes_per_sample;
    status->duration_ms = (status->total_samples * 1000) / g_audio_format.sample_rate;
    status->position_ms = (status->played_samples * 1000) / g_audio_format.sample_rate;
    status->loop_enabled = g_loop_enabled;
    
    return 0;
}

void audio_player_get_dma_stats(uint32_t *half_count, uint32_t *full_count)
{
    if (half_count) *half_count = g_dma_half_count;
    if (full_count) *full_count = g_dma_full_count;
}

void audio_player_get_task_stats(uint32_t *fill_count, uint32_t *timeout_count)
{
    if (fill_count) *fill_count = g_task_fill_count;
    if (timeout_count) *timeout_count = g_task_timeout_count;
}

void audio_player_get_io_stats(uint32_t *read_count, uint32_t *memcpy_count)
{
    if (read_count) *read_count = g_file_read_count;
    if (memcpy_count) *memcpy_count = g_memcpy_count;
}

void audio_player_get_queue_stats(uint32_t *send_count, uint32_t *fail_count)
{
    if (send_count) *send_count = g_queue_send_count;
    if (fail_count) *fail_count = g_queue_send_fail;
}

bool audio_player_is_playing(void)
{
    return (g_player_state == AUDIO_STATE_PLAYING);
}

/********************************************
 * DMA Callbacks (called from ISR context)
 *******************************************/

void audio_player_half_transfer_callback(void)
{
    g_dma_half_count++;
    
    if (g_player_state != AUDIO_STATE_PLAYING) {
        return;
    }
    
    if (g_buffer_queue == NULL) {
        return;
    }
    
    /* Send message to fill first half of buffer */
    BufferFillMsg_t msg = { .buffer_index = 0 };
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(g_buffer_queue, &msg, &xHigherPriorityTaskWoken);
    if (result == pdTRUE) {
        g_queue_send_count++;
    } else {
        g_queue_send_fail++;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void audio_player_transfer_complete_callback(void)
{
    g_dma_full_count++;
    
    if (g_player_state != AUDIO_STATE_PLAYING) {
        return;
    }
    
    if (g_buffer_queue == NULL) {
        return;
    }
    
    /* Send message to fill second half of buffer */
    BufferFillMsg_t msg = { .buffer_index = 1 };
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(g_buffer_queue, &msg, &xHigherPriorityTaskWoken);
    if (result == pdTRUE) {
        g_queue_send_count++;
    } else {
        g_queue_send_fail++;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/********************************************
 * Private functions
 *******************************************/

static void audio_player_task(void *pvParameters)
{
    (void)pvParameters;
    BufferFillMsg_t msg;
    
    SHELL_LOG_AUDIO_DEBUG("Player task started");
    
    while (1) {
        /* Wait for message from DMA callback */
        BaseType_t ret = xQueueReceive(g_buffer_queue, &msg, pdMS_TO_TICKS(500));
        
        if (ret == pdTRUE) {
            if (g_player_state != AUDIO_STATE_PLAYING) {
                continue;
            }
            
            g_task_fill_count++;  /* Count BEFORE fill to detect if stuck in fill_buffer */
            
            /* Fill the requested buffer half */
            uint32_t *buffer = (msg.buffer_index == 0) ? 
                               &g_audio_buffer[0] : 
                               &g_audio_buffer[AUDIO_BUFFER_SIZE];
            
            int fill_ret = fill_buffer(buffer, AUDIO_BUFFER_SAMPLES);
            
            if (fill_ret < 0 && !g_loop_enabled) {
                SHELL_LOG_AUDIO_INFO("End of file reached");
                vTaskDelay(pdMS_TO_TICKS(100));
                audio_player_stop();
                break;
            }
        } else {
            /* Timeout */
            g_task_timeout_count++;
            SHELL_LOG_AUDIO_DEBUG("Task timeout, queue pending=%lu", 
                                  (uint32_t)uxQueueMessagesWaiting(g_buffer_queue));
        }
    }
    
    g_player_task_handle = NULL;
    vTaskDelete(NULL);
}

static int fill_buffer(uint32_t *buffer, uint32_t samples)
{
    FRESULT fres;
    UINT bytes_read;
    uint32_t bytes_to_read;
    uint32_t total_read = 0;
    uint8_t *dest = (uint8_t *)buffer;
    
    if (!g_file_opened) {
        return -1;
    }
    
    /* Calculate bytes to read (32-bit stereo = 8 bytes per sample) */
    bytes_to_read = samples * sizeof(uint32_t) * 2;
    
    /* Read in chunks using intermediate buffer (required for SDMMC DMA alignment) */
    while (total_read < bytes_to_read) {
        uint32_t chunk_size = bytes_to_read - total_read;
        if (chunk_size > FILE_READ_CHUNK_SIZE) {
            chunk_size = FILE_READ_CHUNK_SIZE;
        }
        
        g_file_read_count++;  /* Track before f_read */
        fres = f_read(&g_audio_file, g_file_read_buffer, chunk_size, &bytes_read);
        if (fres != FR_OK) {
            SHELL_LOG_AUDIO_ERROR("File read error: %d at pos %lu", fres, (uint32_t)f_tell(&g_audio_file));
            return -2;
        }
        
        /* Copy to audio buffer - use memcpy for efficiency */
        g_memcpy_count++;  /* Track before memcpy */
        memcpy(dest + total_read, g_file_read_buffer, bytes_read);
        total_read += bytes_read;
        
        /* Check for EOF */
        if (bytes_read < chunk_size) {
            break;
        }
    }
    
    g_played_bytes += total_read;
    
    /* Check if we reached end of file */
    if (total_read < bytes_to_read) {
        /* Fill remaining with silence */
        memset(dest + total_read, 0, bytes_to_read - total_read);
        
        if (g_loop_enabled) {
            /* Seek to beginning for loop playback */
            fres = f_lseek(&g_audio_file, 0);
            if (fres != FR_OK) {
                SHELL_LOG_AUDIO_ERROR("Seek error: %d", fres);
                return -4;
            }
            g_played_bytes = 0;
            SHELL_LOG_AUDIO_DEBUG("Looping...");
            return 0;
        } else {
            return -3;  /* End of file */
        }
    }
    
    return 0;
}

static int start_dma_playback(void)
{
    HAL_StatusTypeDef status;
    
    SHELL_LOG_AUDIO_DEBUG("Starting I2S DMA (circular mode)...");
    SHELL_LOG_AUDIO_DEBUG("TX Buffer addr: 0x%08lX, RX Buffer addr: 0x%08lX", 
                          (uint32_t)g_audio_buffer, (uint32_t)g_audio_rx_buffer);
    
    /* Ensure I2S is in ready state */
    if (hi2s2.State != HAL_I2S_STATE_READY) {
        SHELL_LOG_AUDIO_WARNING("I2S not ready, state=%d, aborting DMA first", hi2s2.State);
        HAL_I2S_DMAStop(&hi2s2);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    /* For Full-Duplex I2S, we must use TransmitReceive even if we don't need RX */
    /* 
     * CRITICAL: DMA is configured with DMA_PDATAALIGN_WORD (32-bit)
     * The HAL Size parameter is number of data items (NOT bytes)
     * Since DMA is 32-bit aligned, each transfer moves 4 bytes
     * 
     * Buffer layout:
     *   g_audio_buffer[AUDIO_BUFFER_SIZE * 2] = [4096] uint32_t = 16384 bytes
     *   Double buffer: first half [0..2047], second half [2048..4095]
     * 
     * Size calculation:
     *   Total 32-bit words = AUDIO_BUFFER_SIZE * 2 = 4096
     *   This will transfer 4096 * 4 = 16384 bytes (correct!)
     */
    uint32_t dma_size = AUDIO_BUFFER_SIZE * 2;  /* 4096 32-bit words = 16384 bytes */
    SHELL_LOG_AUDIO_DEBUG("DMA size: %lu 32-bit words (%lu bytes)", dma_size, dma_size * 4);
    
    status = HAL_I2SEx_TransmitReceive_DMA(&hi2s2, 
                                            (uint16_t *)g_audio_buffer, 
                                            (uint16_t *)g_audio_rx_buffer,
                                            dma_size);
    
    if (status != HAL_OK) {
        SHELL_LOG_AUDIO_ERROR("HAL_I2SEx_TransmitReceive_DMA failed: %d, I2S_State=%d, ErrorCode=0x%lX", 
                              status, hi2s2.State, hi2s2.ErrorCode);
        return -1;
    }
    
    SHELL_LOG_AUDIO_DEBUG("I2S DMA started successfully");
    return 0;
}

static void stop_dma_playback(void)
{
    HAL_I2S_DMAStop(&hi2s2);
    SHELL_LOG_AUDIO_DEBUG("I2S DMA stopped");
}
