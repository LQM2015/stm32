/**
 * @file audio_recorder.c
 * @brief Audio recorder implementation for I2S TDM PCM data recording
 * @date 2025-01-28
 */

/* Includes ------------------------------------------------------------------*/
#include "../Inc/audio_recorder.h"
#include "sai.h"
#include "fatfs.h"
#include "shell_log.h"
#include "fs_manager.h"
#include "diskio.h"  // For SD card status checking
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "cmsis_os.h"
#include "stm32h7xx_hal.h" // For SCB_InvalidateDCache_by_Addr

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static AudioPcmMode_t s_audio_mode = AUDIO_MODE_I2S_STEREO;

static const AudioPcmConfig_t s_pcm_profiles[AUDIO_MODE_COUNT] = {
    [AUDIO_MODE_I2S_STEREO] = {
        .mode = AUDIO_MODE_I2S_STEREO,
        .name = "I2S_STEREO",
        .channels = 2,
        .bit_depth = AUDIO_SUPPORTED_BIT_DEPTH,
        .sample_rate = 16000U,
        .buffer_frames = AUDIO_DMA_BUFFER_FRAMES,
        .sai_protocol = SAI_I2S_STANDARD,
        .sai_datasize = SAI_PROTOCOL_DATASIZE_16BIT,
        .slot_active_mask = (1U << 2) - 1U,
    },
    [AUDIO_MODE_I2S_TDM] = {
        .mode = AUDIO_MODE_I2S_TDM,
        .name = "I2S_TDM",
        .channels = AUDIO_MAX_CHANNELS,
        .bit_depth = AUDIO_SUPPORTED_BIT_DEPTH,
        .sample_rate = 16000U,
        .buffer_frames = AUDIO_DMA_BUFFER_FRAMES,
        .sai_protocol = SAI_PCM_SHORT,
        .sai_datasize = SAI_PROTOCOL_DATASIZE_16BIT,
        .slot_active_mask = (1U << AUDIO_MAX_CHANNELS) - 1U,
    },
};

// 独立的文件对象，避免与录音器结构体混在一起被踩踏
static FIL g_audio_file __attribute__((section(".dma_buffer"))) __attribute__((aligned(32)));

// 音频录制器实例，使用DMA安全的内存区域
#if defined ( __GNUC__ )
__attribute__((section(".dma_buffer"), aligned(32)))
#endif
static AudioRecorder_t recorder = {0};
/* DMA buffer: placed in .dma_buffer (linker -> D3 SRAM 0x38000000) with 32-byte alignment.
 * Region configured via MPU (Region7) as non-cacheable & shareable, so no runtime cache maintenance required.
 * Rationale: BDMA cannot access DTCM; non-cacheable removes need for Clean/Invalidate calls.
 * Buffer size is calculated based on AUDIO_CHANNELS and AUDIO_BUFFER_FRAMES configuration.
 */
#if defined ( __GNUC__ )
__attribute__((section(".dma_buffer"), aligned(32)))
#endif
static uint16_t audio_buffer[AUDIO_DMA_BUFFER_SAMPLES_MAX]; // 16-bit samples interleaved multi-channel data

// 队列数据结构定义 - 简化设计，直接包含数据
typedef struct {
    uint16_t data[AUDIO_DMA_HALF_BUFFER_SIZE_MAX / 2];  // 直接内嵌数据缓冲区 (16-bit samples)
    uint32_t data_size;    // 数据大小（字节）
    uint32_t timestamp;    // 时间戳
    bool is_half_buffer;   // true: 前半缓冲区, false: 后半缓冲区
    uint8_t padding[3];    // 对齐填充
} __attribute__((aligned(32))) AudioDataItem_t;

#define AUDIO_QUEUE_SIZE 6  // 减少队列深度，每个项包含4KB数据

static osMessageQueueId_t audio_data_queue = NULL;
static osThreadId_t audio_process_thread_id = NULL;

// 降低SAI回调打印频次的计数器
static uint32_t rx_complete_counter = 0;
static uint32_t rx_half_counter = 0;
#define RX_CALLBACK_LOG_INTERVAL 50  // 每50次回调打印一次

// External variables from fatfs
extern FATFS USERFatFS;
extern char USERPath[4];

/* Private function prototypes -----------------------------------------------*/
static void generate_filename(char* filename, size_t size);
static int write_audio_data(uint16_t* data, uint32_t size);
static void debug_sai_status(void);
static void monitor_sai_timing(void);
static void reset_sai_timing_status(void);
void audio_recorder_process(void);
static int check_sd_card_status(void);
static void audio_process_thread(void *argument);
static int verify_file_exists(const char* filepath);
static void measure_external_clock_frequency(void);

static const AudioPcmConfig_t* resolve_profile(AudioPcmMode_t mode)
{
    if (mode >= AUDIO_MODE_COUNT) {
        return NULL;
    }
    return &s_pcm_profiles[mode];
}

AudioPcmMode_t audio_recorder_get_mode(void)
{
    return s_audio_mode;
}

const AudioPcmConfig_t* audio_recorder_get_pcm_config(void)
{
    return resolve_profile(s_audio_mode);
}

const AudioPcmConfig_t* audio_recorder_get_pcm_config_for_mode(AudioPcmMode_t mode)
{
    return resolve_profile(mode);
}

uint32_t audio_recorder_get_channel_count(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->channels : 0U;
}

uint32_t audio_recorder_get_sample_rate(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->sample_rate : 0U;
}

uint32_t audio_recorder_get_bit_depth(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->bit_depth : AUDIO_SUPPORTED_BIT_DEPTH;
}

uint32_t audio_recorder_get_bytes_per_frame(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    if (cfg == NULL) {
        return 0U;
    }
    return cfg->channels * (cfg->bit_depth / 8U);
}

uint32_t audio_recorder_get_total_buffer_bytes(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    if (cfg == NULL) {
        return 0U;
    }
    return cfg->buffer_frames * audio_recorder_get_bytes_per_frame();
}

uint32_t audio_recorder_get_half_buffer_bytes(void)
{
    return audio_recorder_get_total_buffer_bytes() / 2U;
}

uint32_t audio_recorder_get_total_buffer_samples(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    if (cfg == NULL || cfg->bit_depth == 0U) {
        return 0U;
    }
    uint32_t bytes_per_sample = cfg->bit_depth / 8U;
    return audio_recorder_get_total_buffer_bytes() / bytes_per_sample;
}

uint32_t audio_recorder_get_half_buffer_samples(void)
{
    return audio_recorder_get_total_buffer_samples() / 2U;
}

uint32_t audio_recorder_get_slot_active_mask(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->slot_active_mask : 0U;
}

uint32_t audio_recorder_get_sai_protocol(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->sai_protocol : SAI_I2S_STANDARD;
}

uint32_t audio_recorder_get_sai_datasize(void)
{
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    return cfg ? cfg->sai_datasize : SAI_PROTOCOL_DATASIZE_16BIT;
}

HAL_StatusTypeDef audio_recorder_set_mode(AudioPcmMode_t mode)
{
    if (mode >= AUDIO_MODE_COUNT) {
        SHELL_LOG_USER_ERROR("Invalid audio mode: %d", mode);
        return HAL_ERROR;
    }

    if ((recorder.state == AUDIO_REC_RECORDING) || (recorder.state == AUDIO_REC_STOPPING)) {
        SHELL_LOG_USER_WARNING("Cannot switch audio mode while recording (state=%d)", recorder.state);
        return HAL_BUSY;
    }

    if (mode == s_audio_mode) {
        return HAL_OK;
    }

    const AudioPcmConfig_t* cfg = resolve_profile(mode);
    if (cfg == NULL) {
        SHELL_LOG_USER_ERROR("Failed to resolve audio profile for mode %d", mode);
        return HAL_ERROR;
    }

    s_audio_mode = mode;

    recorder.channels = cfg->channels;
    recorder.bit_depth = cfg->bit_depth;
    recorder.sample_rate = cfg->sample_rate;
    recorder.buffer_size = audio_recorder_get_total_buffer_bytes();

    if (recorder.file_open) {
        SHELL_LOG_USER_WARNING("Audio file still open while switching mode; closing reference");
        recorder.file_open = false;
    }

    /* Reconfigure SAI peripheral to apply the new audio profile */
    MX_SAI4_Init();

    SHELL_LOG_USER_INFO("Audio mode switched to %s (%d ch, %d-bit, %lu Hz)",
                        cfg->name,
                        cfg->channels,
                        cfg->bit_depth,
                        (unsigned long)cfg->sample_rate);

    return HAL_OK;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Generate filename based on audio parameters
 * @param filename: Buffer to store filename
 * @param size: Buffer size
 */
static void generate_filename(char* filename, size_t size)
{
    // For now, use a simple counter-based naming
    static uint32_t file_counter = 0;
    file_counter++;

    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    uint32_t channels = cfg ? cfg->channels : audio_recorder_get_channel_count();
    uint32_t bit_depth = cfg ? cfg->bit_depth : audio_recorder_get_bit_depth();
    uint32_t sample_rate = cfg ? cfg->sample_rate : audio_recorder_get_sample_rate();
    
    snprintf(filename, size, "audio_%dch_%dbit_%dHz_%03lu.pcm", 
             (int)channels, (int)bit_depth, (int)sample_rate, file_counter);
}

/**
 * @brief Write audio data to SD card
 * @param data: Audio data buffer
 * @param size: Data size in bytes
 * @retval 0: Success, -1: Error
 */
/**
 * @brief Write audio data to SD card with periodic sync
 * @param data: Audio data buffer
 * @param size: Data size in bytes
 * @retval 0: Success, -1: Error
 */
static int write_audio_data(uint16_t* data, uint32_t size)
{
    UINT bytes_written;
    FRESULT res;



    // Check for reentrant calls - prevent interrupt collision
    if (recorder.write_in_progress) {
        // 重入冲突：返回特殊值-2，区别于真正的写入错误-1
        // 禁用频繁的错误日志，避免UART传输影响SD卡操作
        // SHELL_LOG_USER_ERROR("Write operation already in progress - skipping");
        return -2;  // 重入冲突
    }
    
    // Set write-in-progress flag
    recorder.write_in_progress = true;
    
    if (!recorder.file_open) {
        SHELL_LOG_USER_ERROR("Cannot write data, file not open");
        recorder.write_in_progress = false;
        return -1;
    }
    
    // 简化的文件对象有效性检查 - 只检查关键字段
    if (g_audio_file.obj.fs == NULL) {
        SHELL_LOG_USER_ERROR("File object invalid - filesystem pointer is NULL");
        recorder.file_open = false;
        recorder.write_in_progress = false;
        return -1;
    }
    vTaskDelay(4); // 延时以确保文件系统稳定
    // 直接写入，移除中断禁用避免影响SD卡DMA
    res = f_write(&g_audio_file, data, size, &bytes_written);
    
    if (res == FR_OK && bytes_written == size) {
        // Success - update counters
        recorder.bytes_written += bytes_written;

    } else {
        SHELL_LOG_USER_ERROR("Write failed, FRESULT: %d, expected: %lu, written: %lu", 
                            res, (unsigned long)size, (unsigned long)bytes_written);
        recorder.write_in_progress = false;
        return -1;
    }
    
    // Clear write-in-progress flag before returning
    recorder.write_in_progress = false;
    return 0;
}

/**
 * @brief Debug SAI and DMA status
 */
static void debug_sai_status(void)
{
    SHELL_LOG_USER_DEBUG("=== SAI Status Debug ===");
    SHELL_LOG_USER_DEBUG("SAI State: %d", HAL_SAI_GetState(&hsai_BlockA4));
    SHELL_LOG_USER_DEBUG("SAI Error Code: 0x%08lX", hsai_BlockA4.ErrorCode);
    
    // Decode SAI error codes
    if (hsai_BlockA4.ErrorCode != HAL_SAI_ERROR_NONE) {
        SHELL_LOG_USER_ERROR("=== SAI Error Analysis ===");
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_OVR) {
            SHELL_LOG_USER_ERROR("- Overrun Error detected");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_UDR) {
            SHELL_LOG_USER_ERROR("- Underrun Error detected");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_AFSDET) {
            SHELL_LOG_USER_ERROR("- Anticipated Frame Sync Detection Error");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_LFSDET) {
            SHELL_LOG_USER_ERROR("- Late Frame Sync Detection Error");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_CNREADY) {
            SHELL_LOG_USER_ERROR("- Codec Not Ready Error");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_WCKCFG) {
            SHELL_LOG_USER_ERROR("- Wrong Clock Configuration Error");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_TIMEOUT) {
            SHELL_LOG_USER_ERROR("- Timeout Error");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_DMA) {
            SHELL_LOG_USER_ERROR("- DMA Error");
        }
    }
    
    // Check DMA status if available
    if (hsai_BlockA4.hdmarx != NULL) {
        SHELL_LOG_USER_DEBUG("DMA State: %d", HAL_DMA_GetState(hsai_BlockA4.hdmarx));
        SHELL_LOG_USER_DEBUG("DMA Error Code: 0x%08lX", hsai_BlockA4.hdmarx->ErrorCode);
        
        // Decode DMA error codes
        if (hsai_BlockA4.hdmarx->ErrorCode != HAL_DMA_ERROR_NONE) {
            SHELL_LOG_USER_ERROR("=== DMA Error Analysis ===");
            if (hsai_BlockA4.hdmarx->ErrorCode & HAL_DMA_ERROR_TE) {
                SHELL_LOG_USER_ERROR("- DMA Transfer Error");
            }
            if (hsai_BlockA4.hdmarx->ErrorCode & HAL_DMA_ERROR_FE) {
                SHELL_LOG_USER_ERROR("- DMA FIFO Error");
            }
            if (hsai_BlockA4.hdmarx->ErrorCode & HAL_DMA_ERROR_DME) {
                SHELL_LOG_USER_ERROR("- DMA Direct Mode Error");
            }
            if (hsai_BlockA4.hdmarx->ErrorCode & HAL_DMA_ERROR_TIMEOUT) {
                SHELL_LOG_USER_ERROR("- DMA Timeout Error");
            }
        }
    } else {
        SHELL_LOG_USER_WARNING("DMA handle is NULL");
    }
    
    // Print SAI detailed status registers for Late Frame Sync analysis
    uint32_t sai_sr = hsai_BlockA4.Instance->SR;
    uint32_t sai_cr1 = hsai_BlockA4.Instance->CR1;
    uint32_t sai_frcr = hsai_BlockA4.Instance->FRCR;
    uint32_t sai_slotr = hsai_BlockA4.Instance->SLOTR;
    
    SHELL_LOG_USER_INFO("=== SAI Register Analysis ===");
    SHELL_LOG_USER_INFO("SAI4_Block_A Status Register (SR): 0x%08lX", sai_sr);
    SHELL_LOG_USER_INFO("  - FIFO Level: %ld/8", (sai_sr & SAI_xSR_FLVL) >> SAI_xSR_FLVL_Pos);
    SHELL_LOG_USER_INFO("  - Status Flags: %s%s%s%s%s%s%s", 
                        (sai_sr & SAI_xSR_OVRUDR) ? "OVR " : "",
                        (sai_sr & SAI_xSR_MUTEDET) ? "MUTE " : "",
                        (sai_sr & SAI_xSR_WCKCFG) ? "WCKCFG " : "",
                        (sai_sr & SAI_xSR_FREQ) ? "FREQ " : "",
                        (sai_sr & SAI_xSR_CNRDY) ? "CNRDY " : "",
                        (sai_sr & SAI_xSR_AFSDET) ? "AFSDET " : "",
                        (sai_sr & SAI_xSR_LFSDET) ? "LFSDET " : "");
    
    SHELL_LOG_USER_INFO("SAI4_Block_A Control (CR1): 0x%08lX", sai_cr1);
    SHELL_LOG_USER_INFO("  - Mode: %s", (sai_cr1 & SAI_xCR1_MODE) ? "Master" : "Slave");
    SHELL_LOG_USER_INFO("  - Protocol: %s", (sai_cr1 & SAI_xCR1_PRTCFG) ? "Free/Spdif" : "I2S/MSB/LSB/PCM");
    SHELL_LOG_USER_INFO("  - FIFO Threshold: %ld", (hsai_BlockA4.Instance->CR2 & SAI_xCR2_FTH) >> SAI_xCR2_FTH_Pos);
    
    SHELL_LOG_USER_INFO("SAI4_Block_A Frame (FRCR): 0x%08lX", sai_frcr);
    SHELL_LOG_USER_INFO("SAI4_Block_A Slot (SLOTR): 0x%08lX", sai_slotr);
    
    // Check if SAI is currently enabled and receiving data
    SHELL_LOG_USER_INFO("SAI Enabled: %s", (sai_cr1 & SAI_xCR1_SAIEN) ? "YES" : "NO");
    SHELL_LOG_USER_INFO("DMA Enabled: %s", (sai_cr1 & SAI_xCR1_DMAEN) ? "YES" : "NO");
    
    SHELL_LOG_USER_DEBUG("========================");
}

/**
 * @brief Monitor SAI timing status for Late Frame Sync Detection
 */
static void monitor_sai_timing(void)
{
    static uint32_t last_check_time = 0;
    static uint32_t lfsdet_count = 0;
    
    uint32_t current_time = HAL_GetTick();
    
    // Check every 1000ms during recording
    if (recorder.state == AUDIO_REC_RECORDING && 
        (current_time - last_check_time >= 1000)) {
        
        uint32_t sai_sr = hsai_BlockA4.Instance->SR;
        
        // Check for Late Frame Sync Detection flag
        if (sai_sr & SAI_xSR_LFSDET) {
            lfsdet_count++;
            SHELL_LOG_USER_WARNING("LFSDET detected (count: %ld), clearing flag", lfsdet_count);
            
            // Clear the flag to prevent continuous interrupts
            __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_LFSDET);
            
            // If too many LFSDET errors, suggest configuration changes
            if (lfsdet_count >= 3) {
                SHELL_LOG_USER_ERROR("Multiple LFSDET errors detected!");
                SHELL_LOG_USER_ERROR("Possible solutions:");
                SHELL_LOG_USER_ERROR("1. Check external I2S/TDM clock stability");
                SHELL_LOG_USER_ERROR("2. Verify signal integrity (scope trace)");
                SHELL_LOG_USER_ERROR("3. Consider adjusting SAI FIFO threshold");
                SHELL_LOG_USER_ERROR("4. Check for EMI interference");
            }
        }
        
        // Monitor FIFO level for early warning
        uint32_t fifo_level = (sai_sr & SAI_xSR_FLVL) >> SAI_xSR_FLVL_Pos;
        if (fifo_level == 0) {
            SHELL_LOG_USER_WARNING("SAI FIFO empty - possible timing issue");
        } else if (fifo_level >= 7) {
            SHELL_LOG_USER_WARNING("SAI FIFO near full (%ld/8) - possible overrun risk", fifo_level);
        }
        
        last_check_time = current_time;
    }
}

/**
 * @brief Reset SAI timing status and clear error flags
 */
static void reset_sai_timing_status(void)
{
    // Clear any existing SAI error flags
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_LFSDET);
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_AFSDET);
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_CNRDY);
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_FREQ);
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_WCKCFG);
    __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_OVRUDR);
    
    // Reset error code
    hsai_BlockA4.ErrorCode = HAL_SAI_ERROR_NONE;
    
    SHELL_LOG_USER_INFO("SAI timing status reset, all error flags cleared");
}

/**
 * @brief Check SD card status and try to fix common issues
 * @retval 0: Success, -1: Error
 */
static int check_sd_card_status(void)
{
    SHELL_LOG_USER_INFO("=== SD Card Status Check ===");
    
    // 检查全局文件系统管理器状态
    if (fs_manager_check_status() == 0) {
        SHELL_LOG_USER_INFO("SD card is accessible via global fs manager");
        return 0;
    }
    
    SHELL_LOG_USER_WARNING("SD card not accessible via global fs manager");
    
    // 尝试使用文件系统管理器重新挂载
    SHELL_LOG_USER_INFO("Attempting to fix SD card access via fs manager...");
    
    if (fs_manager_remount() == 0) {
        SHELL_LOG_USER_INFO("SD card access restored successfully");
        return 0;
    }
    
    SHELL_LOG_USER_ERROR("Failed to restore SD card access");
    return -1;
}

/**
 * @brief Verify if a file exists on SD card (only when file is not currently open)
 * @param filepath: Path to check
 * @retval 0: File exists, -1: File doesn't exist
 */
static int verify_file_exists(const char* filepath)
{
    FILINFO fno;
    FRESULT res = f_stat(filepath, &fno);
    if (res == FR_OK) {
        SHELL_LOG_USER_INFO("File %s exists, size: %lu bytes", filepath, (unsigned long)fno.fsize);
        return 0;
    } else {
        SHELL_LOG_USER_WARNING("File %s does not exist, FRESULT: %d", filepath, res);
        return -1;
    }
}

/**
 * @brief Measure actual external clock frequency by timing DMA callbacks
 * @note This function starts a temporary recording session to measure the external clock
 */
static void measure_external_clock_frequency(void)
{
    SHELL_LOG_USER_INFO("=== EXTERNAL CLOCK FREQUENCY MEASUREMENT ===");
    
    if (recorder.state != AUDIO_REC_IDLE) {
        SHELL_LOG_USER_WARNING("Cannot measure clock - recorder not idle (state: %d)", recorder.state);
        return;
    }
    
    // 备份原始计数器
    uint32_t backup_rx_complete = rx_complete_counter;
    uint32_t backup_rx_half = rx_half_counter;
    
    // 重置计数器
    rx_complete_counter = 0;
    rx_half_counter = 0;
    
    SHELL_LOG_USER_INFO("Starting temporary measurement session...");
    uint32_t expected_total_samples = audio_recorder_get_total_buffer_samples();
    uint32_t active_sample_rate = audio_recorder_get_sample_rate();
    uint32_t expected_interval_ms = (active_sample_rate > 0U)
                                      ? (expected_total_samples * 1000U) / active_sample_rate
                                      : 0U;
    SHELL_LOG_USER_INFO("Expected timing: %lums per complete callback (%lu samples @ %lu Hz)",
                        (unsigned long)expected_interval_ms,
                        (unsigned long)expected_total_samples,
                        (unsigned long)active_sample_rate);
    
    // 临时开始录音，但不打开文件
    recorder.state = AUDIO_REC_RECORDING;
    
    uint32_t start_time = HAL_GetTick();
    SHELL_LOG_USER_INFO("Measurement start time: %lu ms", start_time);
    
    // 启动SAI DMA
    HAL_StatusTypeDef sai_result = HAL_SAI_Receive_DMA(&hsai_BlockA4,
                                                      (uint8_t*)audio_buffer,
                                                      audio_recorder_get_total_buffer_samples());
    if (sai_result != HAL_OK) {
        SHELL_LOG_USER_ERROR("Failed to start SAI for measurement, result: %d", sai_result);
        recorder.state = AUDIO_REC_IDLE;
        return;
    }
    
    SHELL_LOG_USER_INFO("SAI started, waiting for 5 complete callbacks...");
    
    // 等待5个完整的回调来测量时间
    while (rx_complete_counter < 5) {
        vTaskDelay(100);  // 100ms检查间隔
        
        // 超时保护 - 20秒
        if ((HAL_GetTick() - start_time) > 20000) {
            SHELL_LOG_USER_ERROR("Measurement timeout - no external clock detected");
            break;
        }
    }
    
    uint32_t end_time = HAL_GetTick();
    uint32_t elapsed_ms = end_time - start_time;
    
    // 停止SAI
    HAL_SAI_DMAStop(&hsai_BlockA4);
    recorder.state = AUDIO_REC_IDLE;
    
    SHELL_LOG_USER_INFO("Measurement complete:");
    SHELL_LOG_USER_INFO("- Elapsed time: %lu ms", elapsed_ms);
    SHELL_LOG_USER_INFO("- Complete callbacks: %lu", rx_complete_counter);
    SHELL_LOG_USER_INFO("- Half callbacks: %lu", rx_half_counter);
    
    if (rx_complete_counter >= 2) {
        // 计算平均时间间隔（整数运算）
    uint32_t avg_interval_ms = elapsed_ms / rx_complete_counter;
    uint32_t measured_rate_hz = (avg_interval_ms > 0U)
                    ? (expected_total_samples * 1000U) / avg_interval_ms
                    : 0U;
        
        SHELL_LOG_USER_INFO("Average callback interval: %lu ms", avg_interval_ms);
        SHELL_LOG_USER_INFO("Calculated external sample rate: %lu Hz", measured_rate_hz);
    SHELL_LOG_USER_INFO("Expected: %lu ms interval, %lu Hz rate",
                (unsigned long)expected_interval_ms,
                (unsigned long)active_sample_rate);
        
        if (measured_rate_hz < 8000) {
            SHELL_LOG_USER_ERROR("PROBLEM: External clock rate too low!");
            SHELL_LOG_USER_ERROR("Possible causes:");
            SHELL_LOG_USER_ERROR("1. External I2S/TDM clock source not running");
            SHELL_LOG_USER_ERROR("2. Wrong external clock frequency");
            SHELL_LOG_USER_ERROR("3. SAI should be in master mode instead");
        } else if (measured_rate_hz > 24000) {
            SHELL_LOG_USER_WARNING("External clock rate higher than expected");
        } else {
            SHELL_LOG_USER_INFO("External clock rate within reasonable range");
        }
    } else {
        SHELL_LOG_USER_ERROR("Insufficient callbacks for measurement - check external clock");
    }
    
    // 恢复原始计数器
    rx_complete_counter = backup_rx_complete;
    rx_half_counter = backup_rx_half;
    
    SHELL_LOG_USER_INFO("=== MEASUREMENT COMPLETE ===");
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize audio recorder
 * @retval 0: Success, -1: Error
 */
int audio_recorder_init(void)
{
    SHELL_LOG_USER_INFO("Initializing audio recorder...");
    
    // 清零文件对象确保干净状态，添加内存屏障
    memset(&g_audio_file, 0, sizeof(FIL));
    __DSB();  // 确保内存清零完成
    
    // Initialize recorder structure
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    if (cfg != NULL) {
        recorder.channels = cfg->channels;
        recorder.bit_depth = cfg->bit_depth;
        recorder.sample_rate = cfg->sample_rate;
        recorder.buffer_size = audio_recorder_get_total_buffer_bytes();
    } else {
        recorder.channels = AUDIO_MAX_CHANNELS;
        recorder.bit_depth = AUDIO_SUPPORTED_BIT_DEPTH;
        recorder.sample_rate = 0U;
        recorder.buffer_size = AUDIO_DMA_BUFFER_SIZE_MAX;
    }
    recorder.state = AUDIO_REC_IDLE;
    recorder.bytes_written = 0;
    recorder.file_open = false;
    
    SHELL_LOG_USER_INFO("Config: %d channels, %d-bit, %d Hz, buffer size: %lu", 
           recorder.channels, recorder.bit_depth, recorder.sample_rate, (unsigned long)recorder.buffer_size);
    
    // Clear audio buffer
    memset(audio_buffer, 0, sizeof(audio_buffer));

    // Create message queue for audio data processing
    audio_data_queue = osMessageQueueNew(AUDIO_QUEUE_SIZE, sizeof(AudioDataItem_t), NULL);
    if (audio_data_queue == NULL) {
        SHELL_LOG_USER_ERROR("Failed to create audio data queue");
        return -1;
    }
    SHELL_LOG_USER_INFO("Audio data queue created with size: %d", AUDIO_QUEUE_SIZE);

    // Create audio processing thread
    const osThreadAttr_t audio_process_thread_attributes = {
        .name = "AudioProcess",
        .stack_size = 1024 * 8, // 增加到8KB栈，因为AudioDataItem_t包含大数组
        .priority = (osPriority_t) osPriorityHigh,
    };
    audio_process_thread_id = osThreadNew(audio_process_thread, NULL, &audio_process_thread_attributes);
    if (audio_process_thread_id == NULL) {
        SHELL_LOG_USER_ERROR("Failed to create audio processing thread");
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Audio recorder initialization completed, state: %d", recorder.state);
    return 0;
}

/**
 * @brief Reset audio recorder to clean state (force reset)
 * @retval 0: Success, -1: Error
 */
int audio_recorder_reset(void)
{
    SHELL_LOG_USER_INFO("Force resetting audio recorder...");
    SHELL_LOG_USER_DEBUG("Current state before reset: %d", recorder.state);
    
    // Force stop SAI if it's running
    if (HAL_SAI_GetState(&hsai_BlockA4) != HAL_SAI_STATE_READY) {
        SHELL_LOG_USER_INFO("Forcing SAI DMA stop...");
        HAL_SAI_DMAStop(&hsai_BlockA4);
        HAL_SAI_Abort(&hsai_BlockA4);
        
        // Wait for SAI to stop
        uint32_t timeout = HAL_GetTick() + 200;
        while (HAL_SAI_GetState(&hsai_BlockA4) != HAL_SAI_STATE_READY && HAL_GetTick() < timeout) {
            vTaskDelay(1);
        }
    }
    
    // Force close file if it's open
    if (recorder.file_open) {
        SHELL_LOG_USER_INFO("Force closing file...");
        f_close(&g_audio_file);
        recorder.file_open = false;
    }
    
    // Clear file object
    memset(&g_audio_file, 0, sizeof(FIL));
    __DSB();
    
    // Reset all recorder state
    recorder.state = AUDIO_REC_IDLE;
    recorder.bytes_written = 0;
    recorder.write_in_progress = false;
    
    // Clear queue - remove any pending audio data items
    if (audio_data_queue != NULL) {
        AudioDataItem_t temp_item;
        // 清空队列中的所有待处理数据
        while (osMessageQueueGet(audio_data_queue, &temp_item, NULL, 0) == osOK) {
            // 数据已被丢弃
        }
        SHELL_LOG_USER_DEBUG("Audio data queue cleared");
    }
    
    // Reset counters
    rx_complete_counter = 0;
    rx_half_counter = 0;
    
    SHELL_LOG_USER_INFO("Audio recorder reset completed, state: %d", recorder.state);
    return 0;
}

/**
 * @brief Start audio recording
 * @retval 0: Success, -1: Error
 */
int audio_recorder_start(void)
{
    FRESULT res;
    
    SHELL_LOG_USER_INFO("Starting audio recording...");
    SHELL_LOG_USER_DEBUG("Current state: %d (IDLE=%d, RECORDING=%d, STOPPING=%d, ERROR=%d)", 
                         recorder.state, AUDIO_REC_IDLE, AUDIO_REC_RECORDING, AUDIO_REC_STOPPING, AUDIO_REC_ERROR);
    
    const AudioPcmConfig_t* cfg = audio_recorder_get_pcm_config();
    if (cfg != NULL) {
        recorder.channels = cfg->channels;
        recorder.bit_depth = cfg->bit_depth;
        recorder.sample_rate = cfg->sample_rate;
        recorder.buffer_size = audio_recorder_get_total_buffer_bytes();
    }

    // 如果已经在录音，直接返回错误但用更友好的消息
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_INFO("Audio recording is already active");
        return -1;
    }
    
    // 如果在任何非空闲状态，尝试强制重置
    if (recorder.state != AUDIO_REC_IDLE) {
        SHELL_LOG_USER_WARNING("Recorder not in idle state (%d), attempting force reset...", recorder.state);
        audio_recorder_reset();
        
        // 再次检查状态
        if (recorder.state != AUDIO_REC_IDLE) {
            SHELL_LOG_USER_ERROR("Failed to reset recorder to idle state, current: %d", recorder.state);
            return -1;
        }
        SHELL_LOG_USER_INFO("Recorder successfully reset to idle state");
    }
    
    // Check and fix SD card status first
    if (check_sd_card_status() != 0) {
        SHELL_LOG_USER_ERROR("SD card is not ready for recording");
        recorder.state = AUDIO_REC_ERROR;
        return -1;
    }
    
    // Reset SAI timing status and clear any error flags
    reset_sai_timing_status();

    /* Ensure SAI instance is aligned with the current audio profile */
    MX_SAI4_Init();
    
    // Check if SD card is mounted first
    SHELL_LOG_USER_DEBUG("Checking SD card mount status via global fs manager...");
    
    if (fs_manager_check_status() != 0) {
        SHELL_LOG_USER_ERROR("SD card not accessible via global fs manager");
        
        // 尝试通过文件系统管理器修复
        SHELL_LOG_USER_INFO("Attempting to fix SD card access via global fs manager...");
        
        if (fs_manager_remount() != 0) {
            SHELL_LOG_USER_ERROR("Failed to fix SD card access");
            recorder.state = AUDIO_REC_ERROR;
            return -1;
        }
        
        SHELL_LOG_USER_INFO("SD card access restored via global fs manager");
    } else {
        SHELL_LOG_USER_INFO("SD card is accessible via global fs manager");
    }
    
    // 关键修复：在每次开始录音前重新初始化文件对象
    SHELL_LOG_USER_DEBUG("Reinitializing file object for clean state...");
    memset(&g_audio_file, 0, sizeof(FIL));
    __DSB();  // 内存屏障确保清零完成
    __ISB();  // 指令屏障
    
    // Generate filename
    generate_filename(recorder.filename, sizeof(recorder.filename));
    SHELL_LOG_USER_INFO("Generated filename: %s", recorder.filename);
    
    // Try with directory path
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "%s", recorder.filename);
    SHELL_LOG_USER_INFO("Attempting to open file: %s", full_path);
    
    // Open file for writing (禁用中断保护文件系统操作)
    uint32_t open_primask = __get_PRIMASK();
    __disable_irq();
    
    res = f_open(&g_audio_file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    
    __DSB();  // 确保文件打开完成
    __set_PRIMASK(open_primask);  // 恢复中断
    
    SHELL_LOG_USER_INFO("f_open result: %d for path: %s", res, full_path);
    
    if (res == FR_OK) {
        // 立即同步文件以确保创建 (也需要中断保护)
        uint32_t sync_primask = __get_PRIMASK();
        __disable_irq();
        
        FRESULT sync_res = f_sync(&g_audio_file);
        
        __DSB();
        __set_PRIMASK(sync_primask);
        SHELL_LOG_USER_INFO("f_sync after create result: %d", sync_res);
        
        // 简化：不再写入测试数据，避免扰乱实际PCM文件内容
    }
    
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to open file in / directory, FRESULT: %d", res);
        SHELL_LOG_USER_INFO("Trying to open file in root directory...");
        
        // 第二次尝试也需要中断保护
        uint32_t retry_primask = __get_PRIMASK();
        __disable_irq();
        
        res = f_open(&g_audio_file, recorder.filename, FA_CREATE_ALWAYS | FA_WRITE);
        
        __DSB();
        __set_PRIMASK(retry_primask);
        
        if (res != FR_OK) {
            SHELL_LOG_USER_ERROR("Failed to open file in root directory, FRESULT: %d", res);
            
            // Print detailed error information
            switch(res) {
                case FR_NO_FILE:
                    SHELL_LOG_USER_ERROR("Error: No file found");
                    break;
                case FR_NO_PATH:
                    SHELL_LOG_USER_ERROR("Error: No path found");
                    break;
                case FR_INVALID_NAME:
                    SHELL_LOG_USER_ERROR("Error: Invalid filename");
                    break;
                case FR_DENIED:
                    SHELL_LOG_USER_ERROR("Error: Access denied");
                    break;
                case FR_NOT_READY:
                    SHELL_LOG_USER_ERROR("Error: Drive not ready");
                    break;
                case FR_WRITE_PROTECTED:
                    SHELL_LOG_USER_ERROR("Error: Write protected");
                    break;
                case FR_DISK_ERR:
                    SHELL_LOG_USER_ERROR("Error: Disk error");
                    break;
                case FR_NOT_ENABLED:
                    SHELL_LOG_USER_ERROR("Error: Volume not enabled");
                    break;
                case FR_NO_FILESYSTEM:
                    SHELL_LOG_USER_ERROR("Error: No filesystem");
                    break;
                default:
                    SHELL_LOG_USER_ERROR("Error: Unknown error code %d", res);
                    break;
            }
            
            recorder.state = AUDIO_REC_ERROR;
            return -1;
        } else {
            SHELL_LOG_USER_INFO("File opened successfully in root directory");
            // Keep original filename (no need to copy to itself)
        }
    } else {
        SHELL_LOG_USER_INFO("File opened successfully in / directory");
        strcpy(recorder.filename, full_path); // Update to full path
    }
    
    recorder.file_open = true;
    recorder.bytes_written = 0;
    recorder.write_in_progress = false;  // Initialize write protection flag
    
    // 验证文件是否真的创建成功 - 通过获取文件大小而不是重新打开
    FSIZE_t current_size = f_size(&g_audio_file);
    SHELL_LOG_USER_INFO("File created successfully, current size: %lu bytes", (unsigned long)current_size);
    
    // 记录实际使用的文件路径
    const char* actual_path = (res == FR_OK && strstr(full_path, "audio/")) ? full_path : recorder.filename;
    SHELL_LOG_USER_INFO("File path being used: %s", actual_path);
    
    // 确保状态正确设置为录音中
    recorder.state = AUDIO_REC_RECORDING;
    SHELL_LOG_USER_DEBUG("State set to RECORDING: %d", recorder.state);
    
    // Start SAI DMA reception
    SHELL_LOG_USER_INFO("Starting SAI DMA reception, buffer size: %lu samples (%lu bytes)", 
                        (unsigned long)audio_recorder_get_total_buffer_samples(),
                        (unsigned long)audio_recorder_get_total_buffer_bytes());
    SHELL_LOG_USER_DEBUG("SAI handle: %p, buffer address: %p", &hsai_BlockA4, audio_buffer);
    SHELL_LOG_USER_DEBUG("SAI state before start: %d", HAL_SAI_GetState(&hsai_BlockA4));

    /* Buffer is non-cacheable via MPU; no cache maintenance needed */
    
    HAL_StatusTypeDef sai_result = HAL_SAI_Receive_DMA(&hsai_BlockA4,
                                                      (uint8_t*)audio_buffer,
                                                      audio_recorder_get_total_buffer_samples());
    SHELL_LOG_USER_DEBUG("SAI_Receive_DMA result: %d", sai_result);
    
    if (sai_result != HAL_OK) {
        SHELL_LOG_USER_ERROR("Failed to start SAI DMA reception, HAL result: %d", sai_result);
        SHELL_LOG_USER_DEBUG("SAI state after failed start: %d", HAL_SAI_GetState(&hsai_BlockA4));
        f_close(&g_audio_file);
        recorder.file_open = false;
        recorder.state = AUDIO_REC_ERROR;
        SHELL_LOG_USER_DEBUG("State set to ERROR due to SAI failure: %d", recorder.state);
        return -1;
    }
    
    SHELL_LOG_USER_DEBUG("SAI DMA started successfully");
    SHELL_LOG_USER_DEBUG("SAI state after successful start: %d", HAL_SAI_GetState(&hsai_BlockA4));
    
    // Optional status check (enable by defining AUDIO_REC_VERBOSE)
    vTaskDelay(20);
    debug_sai_status();
    
    // 最终确认状态设置
    if (recorder.state != AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_WARNING("State is not RECORDING after successful start, current: %d", recorder.state);
        recorder.state = AUDIO_REC_RECORDING;
    }
    
    SHELL_LOG_USER_INFO("Recording started successfully, final state: %d", recorder.state);
    SHELL_LOG_USER_DEBUG("Expected RECORDING state value: %d", AUDIO_REC_RECORDING);
    return 0;
}

/**
 * @brief Stop audio recording
 * @retval 0: Success, -1: Error
 */
int audio_recorder_stop(void)
{
    SHELL_LOG_USER_INFO("Stopping audio recording...");
    SHELL_LOG_USER_DEBUG("Current state: %d (IDLE=%d, RECORDING=%d, STOPPING=%d, ERROR=%d)", 
                         recorder.state, AUDIO_REC_IDLE, AUDIO_REC_RECORDING, AUDIO_REC_STOPPING, AUDIO_REC_ERROR);
    
    if (recorder.state == AUDIO_REC_IDLE) {
        SHELL_LOG_USER_INFO("No active recording to stop (already in idle state)");
        return 0; // 成功，没有录音在进行
    }
    
    if (recorder.state == AUDIO_REC_STOPPING) {
        SHELL_LOG_USER_INFO("Recording is already stopping, waiting for completion...");
        
        // 等待停止完成，最多等待1秒
        uint32_t timeout = HAL_GetTick() + 1000;
        while (recorder.state == AUDIO_REC_STOPPING && HAL_GetTick() < timeout) {
            vTaskDelay(10);
        }
        
        if (recorder.state == AUDIO_REC_STOPPING) {
            SHELL_LOG_USER_WARNING("Stop operation timed out, forcing reset");
            audio_recorder_reset();
        }
        
        return 0;
    }
    
    // 允许从任何状态停止，包括错误状态
    if (recorder.state != AUDIO_REC_RECORDING && recorder.state != AUDIO_REC_ERROR) {
        SHELL_LOG_USER_WARNING("Stopping from unexpected state: %d, will attempt to stop anyway", recorder.state);
    }
    
    recorder.state = AUDIO_REC_STOPPING;
    SHELL_LOG_USER_DEBUG("State changed to stopping: %d", recorder.state);
    
    int overall_result = 0;
    
    // Stop SAI DMA with retry mechanism
    SHELL_LOG_USER_INFO("Stopping SAI DMA...");
    HAL_StatusTypeDef dma_result = HAL_ERROR;
    
    // First attempt: Normal stop
    dma_result = HAL_SAI_DMAStop(&hsai_BlockA4);
    
    if (dma_result != HAL_OK) {
        SHELL_LOG_USER_WARNING("First DMA stop attempt failed, result: %d", dma_result);
        
        // Wait a bit for any ongoing transfer to complete
        vTaskDelay(10);
        
        // Second attempt: Force abort
        SHELL_LOG_USER_INFO("Attempting to abort SAI DMA...");
        dma_result = HAL_SAI_Abort(&hsai_BlockA4);
        
        if (dma_result != HAL_OK) {
            SHELL_LOG_USER_ERROR("Failed to abort SAI DMA, result: %d", dma_result);
            overall_result = -1;
        } else {
            SHELL_LOG_USER_INFO("SAI DMA aborted successfully");
        }
    } else {
        SHELL_LOG_USER_INFO("SAI DMA stopped successfully");
    }
    
    // Wait for DMA to fully stop
    uint32_t timeout = HAL_GetTick() + 100; // 100ms timeout
    while (HAL_SAI_GetState(&hsai_BlockA4) != HAL_SAI_STATE_READY && HAL_GetTick() < timeout) {
        vTaskDelay(1);
    }
    
    if (HAL_SAI_GetState(&hsai_BlockA4) != HAL_SAI_STATE_READY) {
        SHELL_LOG_USER_WARNING("SAI not in ready state after stop, current state: %d", HAL_SAI_GetState(&hsai_BlockA4));
    } else {
        SHELL_LOG_USER_DEBUG("SAI is now in ready state");
    }
    
    // Close file with enhanced error handling
    if (recorder.file_open) {
        SHELL_LOG_USER_INFO("Syncing and closing file...");
        
        // First sync the file to ensure all data is written (中断保护)
        uint32_t stop_sync_primask = __get_PRIMASK();
        __disable_irq();
        
        FRESULT sync_result = f_sync(&g_audio_file);
        
        __DSB();
        __set_PRIMASK(stop_sync_primask);
        
        if (sync_result != FR_OK) {
            SHELL_LOG_USER_WARNING("File sync failed, FRESULT: %d", sync_result);
        } else {
            SHELL_LOG_USER_DEBUG("File synced successfully");
        }
        
        // Wait a bit to ensure sync is complete
        vTaskDelay(10);
        
        // Attempt to close file with retry
        FRESULT close_result = FR_DISK_ERR;
        int close_attempts = 0;
        const int max_close_attempts = 3;
        
        while (close_attempts < max_close_attempts && close_result != FR_OK) {
            close_attempts++;
            SHELL_LOG_USER_DEBUG("File close attempt %d/%d", close_attempts, max_close_attempts);
            
            // 关闭文件也需要中断保护
            uint32_t close_primask = __get_PRIMASK();
            __disable_irq();
            
            close_result = f_close(&g_audio_file);
            
            __DSB();
            __set_PRIMASK(close_primask);
            
            if (close_result == FR_OK) {
                SHELL_LOG_USER_INFO("File closed successfully on attempt %d", close_attempts);
                break;
            } else {
                SHELL_LOG_USER_WARNING("File close attempt %d failed, FRESULT: %d", close_attempts, close_result);
                
                // Print detailed error information
                switch(close_result) {
                    case FR_DISK_ERR:
                        SHELL_LOG_USER_ERROR("Disk error during file close");
                        break;
                    case FR_INT_ERR:
                        SHELL_LOG_USER_ERROR("Internal error during file close");
                        break;
                    case FR_NOT_READY:
                        SHELL_LOG_USER_ERROR("Drive not ready during file close");
                        break;
                    case FR_INVALID_OBJECT:
                        SHELL_LOG_USER_ERROR("Invalid file object during close");
                        break;
                    case FR_WRITE_PROTECTED:
                        SHELL_LOG_USER_ERROR("Write protected during file close");
                        break;
                    default:
                        SHELL_LOG_USER_ERROR("Unknown error %d during file close", close_result);
                        break;
                }
                
                if (close_attempts < max_close_attempts) {
                    vTaskDelay(50); // Wait before retry
                }
            }
        }
        
        if (close_result != FR_OK) {
            SHELL_LOG_USER_ERROR("Failed to close file after %d attempts, FRESULT: %d", max_close_attempts, close_result);
            overall_result = -1;
        } else {
            SHELL_LOG_USER_INFO("File handle closed successfully");
        }
        
        recorder.file_open = false;
        
    }
    
    // Always transition to idle state, even if there were errors
    recorder.state = AUDIO_REC_IDLE;
    SHELL_LOG_USER_INFO("Recording stopped, total bytes written: %lu", (unsigned long)recorder.bytes_written);
    SHELL_LOG_USER_DEBUG("State changed to idle: %d", recorder.state);
    
    // 现在文件已关闭，可以安全验证文件是否存在
    SHELL_LOG_USER_INFO("Verifying recorded file exists on SD card...");
    if (verify_file_exists(recorder.filename) == 0) {
        SHELL_LOG_USER_INFO("File verification successful - recorded file found on SD card");
    } else {
        SHELL_LOG_USER_ERROR("File verification failed - recorded file not found on SD card!");
    }
    
    if (overall_result == 0) {
        SHELL_LOG_USER_INFO("Audio recording stopped successfully");
    } else {
        SHELL_LOG_USER_WARNING("Audio recording stopped with errors");
    }
    
    return overall_result;
}

/**
 * @brief Get current recorder state
 * @retval Current state
 */
AudioRecorderState_t audio_recorder_get_state(void)
{
    return recorder.state;
}

/**
 * @brief Get total bytes written
 * @retval Bytes written
 */
uint32_t audio_recorder_get_bytes_written(void)
{
    return recorder.bytes_written;
}

/**
 * @brief Get current filename
 * @retval Filename string
 */
const char* audio_recorder_get_filename(void)
{
    return recorder.filename;
}

/**
 * @brief Debug current audio recorder status
 */
void audio_recorder_debug_status(void)
{
    SHELL_LOG_USER_INFO("=== Audio Recorder Status ===");
    SHELL_LOG_USER_INFO("State: %d", recorder.state);
    SHELL_LOG_USER_INFO("Channels: %d", recorder.channels);
    SHELL_LOG_USER_INFO("Bit depth: %d", recorder.bit_depth);
    SHELL_LOG_USER_INFO("Sample rate: %d", recorder.sample_rate);
    SHELL_LOG_USER_INFO("Buffer size: %lu", (unsigned long)recorder.buffer_size);
    SHELL_LOG_USER_INFO("Bytes written: %lu", (unsigned long)recorder.bytes_written);
    SHELL_LOG_USER_INFO("File open: %s", recorder.file_open ? "Yes" : "No");
    SHELL_LOG_USER_INFO("Filename: %s", recorder.filename);
    
    // 显示队列状态信息
    if (audio_data_queue != NULL) {
        uint32_t queue_space = osMessageQueueGetSpace(audio_data_queue);
        uint32_t queue_count = osMessageQueueGetCount(audio_data_queue);
        SHELL_LOG_USER_INFO("Audio queue status: %lu/%d items (space: %lu)", 
                           queue_count, AUDIO_QUEUE_SIZE, queue_space);
        
        if (queue_count >= (AUDIO_QUEUE_SIZE * 3 / 4)) {
            SHELL_LOG_USER_WARNING("Audio queue is nearly full - potential data loss risk!");
        }
    } else {
        SHELL_LOG_USER_ERROR("Audio queue is NULL!");
    }
    
    // Always check SD card status
    check_sd_card_status();
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        debug_sai_status();
        
        // 检查当前缓冲区内容
        int current_non_zero = 0;
        uint32_t total_samples = audio_recorder_get_total_buffer_samples();
        for (uint32_t i = 0; (i < 10U) && (i < total_samples); i++) {
            if (audio_buffer[i] != 0) current_non_zero++;
        }
        SHELL_LOG_USER_INFO("Current non-zero samples in first 10: %d", current_non_zero);
        SHELL_LOG_USER_INFO("First few samples: 0x%04X 0x%04X 0x%04X 0x%04X", 
                           audio_buffer[0], audio_buffer[1], audio_buffer[2], audio_buffer[3]);
    }
    
    SHELL_LOG_USER_INFO("=============================");
}

/**
 * @brief Check SD card status
 * @retval 0: Success, -1: Error
 */
int audio_recorder_check_sd_card(void)
{
    return check_sd_card_status();
}

/**
 * @brief Measure external I2S/TDM clock frequency
 * @note This helps diagnose timing issues and verify external clock source
 */
void audio_recorder_measure_clock(void)
{
    measure_external_clock_frequency();
}

/**
 * @brief Audio processing thread function
 * @param argument: Not used
 */
static void audio_process_thread(void *argument)
{
    (void)argument;
    AudioDataItem_t audio_item;
    
    SHELL_LOG_USER_INFO("Audio processing thread started");
    
    for (;;) {
        // Wait for audio data from the queue (blocking)
        osStatus_t status = osMessageQueueGet(audio_data_queue, &audio_item, NULL, osWaitForever);
        
        if (status == osOK) {
            //SHELL_LOG_USER_DEBUG("Processing audio data: %lu bytes", (unsigned long)audio_item.data_size);
            
            // Check if we're still recording
            if (recorder.state == AUDIO_REC_RECORDING) {
                // Process the received audio data
                int write_result = write_audio_data(audio_item.data, audio_item.data_size);
                
                if (write_result == -1) {
                    // 真正的写入错误
                    SHELL_LOG_USER_ERROR("Failed to write %s buffer, stopping recording.", 
                                        audio_item.is_half_buffer ? "first half" : "second half");
                    audio_recorder_stop();
                    recorder.state = AUDIO_REC_ERROR;
                } else if (write_result == -2) {
                    // 重入冲突，跳过但不设置错误状态
                    SHELL_LOG_USER_DEBUG("%s buffer write skipped due to reentry conflict",
                                        audio_item.is_half_buffer ? "First half" : "Second half");
                } else {
                    // 写入成功
                    // SHELL_LOG_USER_DEBUG("%s buffer written successfully",
                    //                     audio_item.is_half_buffer ? "First half" : "Second half");
                }
            } else {
                // 不在录音状态，丢弃数据
                SHELL_LOG_USER_DEBUG("Discarding audio data - not in recording state (state=%d)", recorder.state);
            }
        } else {
            SHELL_LOG_USER_WARNING("Failed to get audio data from queue, status: %d", status);
            // 短暂延时后继续
            osDelay(1);
        }
    }
}

/**
 * @brief Process audio data buffers. 
 * @note This function is now simplified as audio processing is handled by the queue-based thread.
 *       It only monitors SAI timing status.
 */
void audio_recorder_process(void)
{
    if (recorder.state != AUDIO_REC_RECORDING) {
        return;
    }

    // Monitor SAI timing status for Late Frame Sync issues
    monitor_sai_timing();
    
    // Note: Audio data processing is now handled by the audio_process_thread 
    // via queue mechanism for better real-time performance and data integrity.
}

/* Callback functions --------------------------------------------------------*/

/**
 * @brief SAI RX complete callback
 */
void audio_recorder_rx_complete_callback(void)
{
    /* Keep callbacks lightweight: reduced debug log frequency */
    rx_complete_counter++;
    if (rx_complete_counter % RX_CALLBACK_LOG_INTERVAL == 0) {
        SHELL_LOG_USER_DEBUG("SAI RX complete (%lu)", (unsigned long)rx_complete_counter);
    }
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        //SHELL_LOG_USER_DEBUG("RX complete callback triggered");
        
        // 准备队列数据项 - 后半缓冲区
        AudioDataItem_t audio_item;
        
    uint32_t half_bytes = audio_recorder_get_half_buffer_bytes();
    uint32_t half_samples = audio_recorder_get_half_buffer_samples();

    // 复制DMA数据到独立缓冲区，避免被硬件覆盖
    uint16_t* source_ptr = &audio_buffer[half_samples];  // 后半缓冲区
    memcpy(audio_item.data, source_ptr, half_bytes);
        __DSB();  // 确保内存复制完成
        
    audio_item.data_size = half_bytes;
        audio_item.timestamp = HAL_GetTick();
        audio_item.is_half_buffer = false;  // 完整回调 = 后半缓冲区
        
        // 非阻塞方式推送到队列
        osStatus_t status = osMessageQueuePut(audio_data_queue, &audio_item, 0, 0);
        if (status != osOK) {
            // 队列满了，数据丢失
            static uint32_t queue_full_count = 0;
            queue_full_count++;
            if (queue_full_count % 10 == 1) {  // 每10次打印一次警告
                SHELL_LOG_USER_WARNING("Audio queue full in RX complete callback! Lost count: %lu", queue_full_count);
            }
        } else {
            //SHELL_LOG_USER_DEBUG("Data queued successfully in RX complete");
        }
    }
}

/**
 * @brief SAI RX half complete callback
 */
void audio_recorder_rx_half_complete_callback(void)
{
    /* Half complete - reduced debug log frequency */
    rx_half_counter++;
    if (rx_half_counter % RX_CALLBACK_LOG_INTERVAL == 0) {
        SHELL_LOG_USER_DEBUG("SAI RX half (%lu)", (unsigned long)rx_half_counter);
    }
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        // 准备队列数据项 - 前半缓冲区
        AudioDataItem_t audio_item;
        
    uint32_t half_bytes = audio_recorder_get_half_buffer_bytes();

    // 复制DMA数据到独立缓冲区，避免被硬件覆盖
    uint16_t* source_ptr = &audio_buffer[0];  // 前半缓冲区
    memcpy(audio_item.data, source_ptr, half_bytes);
        __DSB();  // 确保内存复制完成
        
    audio_item.data_size = half_bytes;
        audio_item.timestamp = HAL_GetTick();
        audio_item.is_half_buffer = true;   // 半完成回调 = 前半缓冲区
        
        // 非阻塞方式推送到队列
        osStatus_t status = osMessageQueuePut(audio_data_queue, &audio_item, 0, 0);
        if (status != osOK) {
            // 队列满了，数据丢失
            static uint32_t queue_full_count_half = 0;
            queue_full_count_half++;
            if (queue_full_count_half % 10 == 1) {  // 每10次打印一次警告
                SHELL_LOG_USER_WARNING("Audio queue full in RX half callback! Lost count: %lu", queue_full_count_half);
            }
        }
    }
}

/**
 * @brief SAI error callback
 */
void audio_recorder_error_callback(void)
{
    SHELL_LOG_USER_ERROR("SAI error callback triggered! (state=%d)", recorder.state);
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_ERROR("SAI error during recording, stopping...");
        
        // Print detailed status at the moment of error
        debug_sai_status();
        
        // Check and provide specific error guidance
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_LFSDET) {
            SHELL_LOG_USER_ERROR("Late Frame Sync detected");
            SHELL_LOG_USER_ERROR("Suggestions:");
            SHELL_LOG_USER_ERROR("1. Check external I2S clock stability");
            SHELL_LOG_USER_ERROR("2. Verify SAI configuration matches hardware");
            SHELL_LOG_USER_ERROR("3. Consider reducing DMA transfer size");
            SHELL_LOG_USER_ERROR("4. Check for SD card write bottlenecks");
            
            // Try to clear the error and continue if possible
            __HAL_SAI_CLEAR_FLAG(&hsai_BlockA4, SAI_FLAG_LFSDET);
            hsai_BlockA4.ErrorCode &= ~HAL_SAI_ERROR_LFSDET;
            
            // Don't immediately stop - try to recover
            SHELL_LOG_USER_WARNING("Attempting to continue recording after LFSDET error");
            return;
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_AFSDET) {
            SHELL_LOG_USER_ERROR("Anticipated Frame Sync detected - frame timing issue");
            SHELL_LOG_USER_ERROR("Suggestion: Verify SAI frame configuration and external codec");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_OVR) {
            SHELL_LOG_USER_ERROR("SAI Overrun - data not read fast enough");
            SHELL_LOG_USER_ERROR("Suggestion: Check DMA configuration and buffer handling");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_UDR) {
            SHELL_LOG_USER_ERROR("SAI Underrun - data not supplied fast enough");
            SHELL_LOG_USER_ERROR("Suggestion: Check DMA configuration and data flow");
        }
        if (hsai_BlockA4.ErrorCode & HAL_SAI_ERROR_WCKCFG) {
            SHELL_LOG_USER_ERROR("Wrong Clock Configuration detected");
            SHELL_LOG_USER_ERROR("Suggestion: Verify SAI clock settings and PLL configuration");
        }
        
        recorder.state = AUDIO_REC_ERROR;
        
        // Stop DMA and close file
        HAL_SAI_DMAStop(&hsai_BlockA4);
        // if (recorder.file_open) {
        //     f_close(&recorder.file);
        //     recorder.file_open = false;
        // }
        
        SHELL_LOG_USER_ERROR("Recording stopped due to SAI error");
    }
}
