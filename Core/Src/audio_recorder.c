/**
 * @file audio_recorder.c
 * @brief Audio recorder implementation for I2S TDM PCM data recording
 * @date 2025-01-28
 */

/* Includes ------------------------------------------------------------------*/
#include "audio_recorder.h"
#include "sai.h"
#include "fatfs.h"
#include "shell_log.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static AudioRecorder_t recorder = {0};
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE / 2]; // 16-bit samples
static volatile bool buffer_ready = false;
static volatile bool half_buffer_ready = false;

// External variables from fatfs
extern FATFS USERFatFS;
extern char USERPath[4];

/* Private function prototypes -----------------------------------------------*/
static void generate_filename(char* filename, size_t size);
static int write_audio_data(uint16_t* data, uint32_t size);

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
    
    snprintf(filename, size, "audio_%dch_%dbit_%dHz_%03lu.pcm", 
             AUDIO_CHANNELS, AUDIO_BIT_DEPTH, AUDIO_SAMPLE_RATE, file_counter);
}

/**
 * @brief Write audio data to SD card
 * @param data: Audio data buffer
 * @param size: Data size in bytes
 * @retval 0: Success, -1: Error
 */
static int write_audio_data(uint16_t* data, uint32_t size)
{
    UINT bytes_written;
    FRESULT res;
    
    if (!recorder.file_open) {
        SHELL_LOG_USER_ERROR("Cannot write data, file not open");
        return -1;
    }
    
    res = f_write(&recorder.file, data, size, &bytes_written);
    if (res != FR_OK || bytes_written != size) {
        SHELL_LOG_USER_ERROR("Write failed, FRESULT: %d, requested: %lu, written: %lu", 
               res, (unsigned long)size, (unsigned long)bytes_written);
        return -1;
    }
    
    recorder.bytes_written += bytes_written;
    
    // Sync file periodically to ensure data is written
    if (recorder.bytes_written % (AUDIO_BUFFER_SIZE * 10) == 0) {
        SHELL_LOG_USER_DEBUG("Syncing file, total written: %lu bytes", (unsigned long)recorder.bytes_written);
        f_sync(&recorder.file);
    }
    
    return 0;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize audio recorder
 * @retval 0: Success, -1: Error
 */
int audio_recorder_init(void)
{
    SHELL_LOG_USER_INFO("Initializing audio recorder...");
    
    // Initialize recorder structure
    recorder.channels = AUDIO_CHANNELS;
    recorder.bit_depth = AUDIO_BIT_DEPTH;
    recorder.sample_rate = AUDIO_SAMPLE_RATE;
    recorder.state = AUDIO_REC_IDLE;
    recorder.buffer_size = AUDIO_BUFFER_SIZE;
    recorder.bytes_written = 0;
    recorder.file_open = false;
    
    SHELL_LOG_USER_INFO("Config: %d channels, %d-bit, %d Hz, buffer size: %lu", 
           recorder.channels, recorder.bit_depth, recorder.sample_rate, (unsigned long)recorder.buffer_size);
    
    // Clear audio buffer
    memset(audio_buffer, 0, sizeof(audio_buffer));
    
    SHELL_LOG_USER_INFO("Audio recorder initialization completed, state: %d", recorder.state);
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
    SHELL_LOG_USER_DEBUG("Current state: %d", recorder.state);
    
    if (recorder.state != AUDIO_REC_IDLE) {
        SHELL_LOG_USER_ERROR("Cannot start recording, not in idle state");
        return -1; // Already recording or in error state
    }
    
    // Check if SD card is mounted first
    SHELL_LOG_USER_DEBUG("Checking SD card mount status...");
    FATFS *fs;
    DWORD fre_clust;
    res = f_getfree("0:", &fre_clust, &fs);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("SD card not mounted or accessible, FRESULT: %d", res);
        SHELL_LOG_USER_INFO("Attempting to mount SD card...");
        res = f_mount(&USERFatFS, USERPath, 1);
        if (res != FR_OK) {
            SHELL_LOG_USER_ERROR("Failed to mount SD card, FRESULT: %d", res);
            recorder.state = AUDIO_REC_ERROR;
            return -1;
        }
        SHELL_LOG_USER_INFO("SD card mounted successfully");
    } else {
        SHELL_LOG_USER_INFO("SD card is accessible, free clusters: %lu", fre_clust);
    }
    
    // Generate filename
    generate_filename(recorder.filename, sizeof(recorder.filename));
    SHELL_LOG_USER_INFO("Generated filename: %s", recorder.filename);
    
    // Try to create directory if needed
    SHELL_LOG_USER_DEBUG("Ensuring directory exists...");
    res = f_mkdir("audio");
    if (res == FR_OK) {
        SHELL_LOG_USER_INFO("Created audio directory");
    } else if (res == FR_EXIST) {
        SHELL_LOG_USER_DEBUG("Audio directory already exists");
    } else {
        SHELL_LOG_USER_DEBUG("Could not create audio directory, FRESULT: %d", res);
    }
    
    // Try with directory path
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "audio/%s", recorder.filename);
    SHELL_LOG_USER_INFO("Attempting to open file: %s", full_path);
    
    // Open file for writing
    res = f_open(&recorder.file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to open file in audio/ directory, FRESULT: %d", res);
        SHELL_LOG_USER_INFO("Trying to open file in root directory...");
        res = f_open(&recorder.file, recorder.filename, FA_CREATE_ALWAYS | FA_WRITE);
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
        SHELL_LOG_USER_INFO("File opened successfully in audio/ directory");
        strcpy(recorder.filename, full_path); // Update to full path
    }
    
    recorder.file_open = true;
    recorder.bytes_written = 0;
    recorder.state = AUDIO_REC_RECORDING;
    
    // Start SAI DMA reception
    SHELL_LOG_USER_INFO("Starting SAI DMA reception, buffer size: %d", AUDIO_BUFFER_SIZE / 2);
    if (HAL_SAI_Receive_DMA(&hsai_BlockA4, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE / 2) != HAL_OK) {
        SHELL_LOG_USER_ERROR("Failed to start SAI DMA reception");
        f_close(&recorder.file);
        recorder.file_open = false;
        recorder.state = AUDIO_REC_ERROR;
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Recording started successfully, state: %d", recorder.state);
    return 0;
}

/**
 * @brief Stop audio recording
 * @retval 0: Success, -1: Error
 */
int audio_recorder_stop(void)
{
    SHELL_LOG_USER_INFO("Stopping audio recording...");
    SHELL_LOG_USER_DEBUG("Current state: %d", recorder.state);
    
    if (recorder.state != AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_ERROR("Cannot stop recording, not in recording state");
        return -1;
    }
    
    recorder.state = AUDIO_REC_STOPPING;
    SHELL_LOG_USER_DEBUG("State changed to stopping: %d", recorder.state);
    
    // Stop SAI DMA
    SHELL_LOG_USER_INFO("Stopping SAI DMA...");
    HAL_StatusTypeDef dma_result = HAL_SAI_DMAStop(&hsai_BlockA4);
    if (dma_result == HAL_OK) {
        SHELL_LOG_USER_INFO("SAI DMA stopped successfully");
    } else {
        SHELL_LOG_USER_ERROR("Failed to stop SAI DMA, result: %d", dma_result);
    }
    
    // Close file
    if (recorder.file_open) {
        SHELL_LOG_USER_INFO("Syncing and closing file...");
        f_sync(&recorder.file);
        FRESULT close_result = f_close(&recorder.file);
        if (close_result == FR_OK) {
            SHELL_LOG_USER_INFO("File closed successfully");
        } else {
            SHELL_LOG_USER_ERROR("Failed to close file, FRESULT: %d", close_result);
        }
        recorder.file_open = false;
    }
    
    recorder.state = AUDIO_REC_IDLE;
    SHELL_LOG_USER_INFO("Recording stopped, total bytes written: %lu", (unsigned long)recorder.bytes_written);
    SHELL_LOG_USER_DEBUG("State changed to idle: %d", recorder.state);
    
    return 0;
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

/* Callback functions --------------------------------------------------------*/

/**
 * @brief SAI RX complete callback
 */
void audio_recorder_rx_complete_callback(void)
{
    SHELL_LOG_USER_DEBUG("RX complete callback triggered");
    SHELL_LOG_USER_DEBUG("Current state: %d", recorder.state);
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_DEBUG("Writing second half of buffer, size: %d bytes", AUDIO_BUFFER_SIZE / 2);
        // Write second half of buffer
        int result = write_audio_data(&audio_buffer[AUDIO_BUFFER_SIZE / 4], AUDIO_BUFFER_SIZE / 2);
        if (result == 0) {
            SHELL_LOG_USER_DEBUG("Second half buffer written successfully");
        } else {
            SHELL_LOG_USER_ERROR("Failed to write second half buffer");
        }
    } else {
        SHELL_LOG_USER_WARNING("RX complete callback called but not recording");
    }
}

/**
 * @brief SAI RX half complete callback
 */
void audio_recorder_rx_half_complete_callback(void)
{
    SHELL_LOG_USER_DEBUG("RX half complete callback triggered");
    SHELL_LOG_USER_DEBUG("Current state: %d", recorder.state);
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_DEBUG("Writing first half of buffer, size: %d bytes", AUDIO_BUFFER_SIZE / 2);
        // Write first half of buffer
        int result = write_audio_data(&audio_buffer[0], AUDIO_BUFFER_SIZE / 2);
        if (result == 0) {
            SHELL_LOG_USER_DEBUG("First half buffer written successfully");
        } else {
            SHELL_LOG_USER_ERROR("Failed to write first half buffer");
        }
    } else {
        SHELL_LOG_USER_WARNING("RX half complete callback called but not recording");
    }
}

/**
 * @brief SAI error callback
 */
void audio_recorder_error_callback(void)
{
    SHELL_LOG_USER_ERROR("SAI error callback triggered");
    SHELL_LOG_USER_DEBUG("Current state: %d", recorder.state);
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_ERROR("SAI error during recording, stopping...");
        recorder.state = AUDIO_REC_ERROR;
        
        // Stop DMA and close file
        HAL_SAI_DMAStop(&hsai_BlockA4);
        if (recorder.file_open) {
            f_close(&recorder.file);
            recorder.file_open = false;
        }
        
        SHELL_LOG_USER_ERROR("Recording stopped due to SAI error");
    }
}