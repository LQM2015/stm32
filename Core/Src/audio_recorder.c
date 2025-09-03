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
static AudioRecorder_t recorder = {0};
/* DMA buffer: placed in .dma_buffer (linker -> D3 SRAM 0x38000000) with 32-byte alignment.
 * Region configured via MPU (Region7) as non-cacheable & shareable, so no runtime cache maintenance required.
 * Rationale: BDMA cannot access DTCM; non-cacheable removes need for Clean/Invalidate calls.
 */
#if defined ( __GNUC__ )
__attribute__((section(".dma_buffer"), aligned(32)))
#endif
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE / 2]; // 16-bit samples interleaved 8-channel TDM
static volatile bool buffer_ready = false;
static volatile bool half_buffer_ready = false;

static osSemaphoreId_t audio_data_sem = NULL;
static osThreadId_t audio_process_thread_id = NULL;

// External variables from fatfs
extern FATFS USERFatFS;
extern char USERPath[4];

/* Private function prototypes -----------------------------------------------*/
static void generate_filename(char* filename, size_t size);
static int write_audio_data(uint16_t* data, uint32_t size);
static void debug_sai_status(void);
static int check_sd_card_status(void);
static void audio_process_thread(void *argument);
static int verify_file_exists(const char* filepath);

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
    
    // Sync file periodically to ensure data is written.
    // Increased the interval to reduce the frequency of potentially long, blocking f_sync calls,
    // which might interfere with time-sensitive peripherals like SAI.
    if (recorder.bytes_written > 0 && recorder.bytes_written % (AUDIO_BUFFER_SIZE * 100) == 0) {
        SHELL_LOG_USER_DEBUG("Syncing file, total written: %lu bytes", (unsigned long)recorder.bytes_written);
        f_sync(&recorder.file);
    }
    
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
    
    // Check DMA status if available
    if (hsai_BlockA4.hdmarx != NULL) {
        SHELL_LOG_USER_DEBUG("DMA State: %d", HAL_DMA_GetState(hsai_BlockA4.hdmarx));
        SHELL_LOG_USER_DEBUG("DMA Error Code: 0x%08lX", hsai_BlockA4.hdmarx->ErrorCode);
    } else {
        SHELL_LOG_USER_WARNING("DMA handle is NULL");
    }
    
    SHELL_LOG_USER_DEBUG("========================");
}

/**
 * @brief Check SD card status and try to fix common issues
 * @retval 0: Success, -1: Error
 */
static int check_sd_card_status(void)
{
    FRESULT res;
    FATFS *fs;
    DWORD fre_clust;
    
    SHELL_LOG_USER_INFO("=== SD Card Status Check ===");
    
    // Try to get free space (this tests if SD card is accessible)
    res = f_getfree("0:", &fre_clust, &fs);
    if (res == FR_OK) {
        SHELL_LOG_USER_INFO("SD card is accessible");
        SHELL_LOG_USER_INFO("Free clusters: %lu", fre_clust);
        SHELL_LOG_USER_INFO("Cluster size: %lu bytes", fs->csize * 512);
        SHELL_LOG_USER_INFO("Free space: %lu MB", (fre_clust * fs->csize) / 2048);
        return 0;
    }
    
    SHELL_LOG_USER_WARNING("SD card not accessible, FRESULT: %d", res);
    
    // Try to fix by unmounting and remounting
    SHELL_LOG_USER_INFO("Attempting to fix SD card access...");
    
    // Unmount first
    f_mount(NULL, "0:", 0);
    HAL_Delay(200);
    
    // Try to mount again
    res = f_mount(&USERFatFS, "0:", 1);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to remount SD card, FRESULT: %d", res);
        return -1;
    }
    
    // Verify the mount worked
    HAL_Delay(100);
    res = f_getfree("0:", &fre_clust, &fs);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("SD card still not accessible after remount, FRESULT: %d", res);
        return -1;
    }
    
    SHELL_LOG_USER_INFO("SD card access restored successfully");
    SHELL_LOG_USER_INFO("Free clusters: %lu", fre_clust);
    return 0;
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

    // Create semaphore for data processing
    audio_data_sem = osSemaphoreNew(1, 0, NULL);
    if (audio_data_sem == NULL) {
        SHELL_LOG_USER_ERROR("Failed to create audio data semaphore");
        return -1;
    }

    // Create audio processing thread
    const osThreadAttr_t audio_process_thread_attributes = {
        .name = "AudioProcess",
        .stack_size = 512 * 4, // 2KB stack
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
 * @brief Start audio recording
 * @retval 0: Success, -1: Error
 */
int audio_recorder_start(void)
{
    FRESULT res;
    
    SHELL_LOG_USER_INFO("Starting audio recording...");
    SHELL_LOG_USER_DEBUG("Current state: %d (IDLE=%d, RECORDING=%d, STOPPING=%d, ERROR=%d)", 
                         recorder.state, AUDIO_REC_IDLE, AUDIO_REC_RECORDING, AUDIO_REC_STOPPING, AUDIO_REC_ERROR);
    
    // 如果在错误状态，先重置到空闲状态
    if (recorder.state == AUDIO_REC_ERROR) {
        SHELL_LOG_USER_WARNING("Recorder in error state, resetting to idle");
        recorder.state = AUDIO_REC_IDLE;
        recorder.file_open = false;
    }
    
    // 如果在停止状态，等待完成或强制重置
    if (recorder.state == AUDIO_REC_STOPPING) {
        SHELL_LOG_USER_WARNING("Recorder in stopping state, forcing reset to idle");
        recorder.state = AUDIO_REC_IDLE;
        recorder.file_open = false;
    }
    
    if (recorder.state != AUDIO_REC_IDLE) {
        SHELL_LOG_USER_ERROR("Cannot start recording, current state: %d (expected IDLE=%d)", 
                             recorder.state, AUDIO_REC_IDLE);
        return -1; // Already recording
    }
    
    // Check and fix SD card status first
    if (check_sd_card_status() != 0) {
        SHELL_LOG_USER_ERROR("SD card is not ready for recording");
        recorder.state = AUDIO_REC_ERROR;
        return -1;
    }
    
    // Check if SD card is mounted first
    SHELL_LOG_USER_DEBUG("Checking SD card mount status...");
    FATFS *fs;
    DWORD fre_clust;
    res = f_getfree("0:", &fre_clust, &fs);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("SD card not mounted or accessible, FRESULT: %d", res);
        
        // Print detailed error information for f_getfree
        switch(res) {
            case FR_NOT_READY:
                SHELL_LOG_USER_ERROR("Drive not ready - SD card may not be inserted or initialized");
                break;
            case FR_DISK_ERR:
                SHELL_LOG_USER_ERROR("Disk error - SD card hardware issue");
                break;
            case FR_NOT_ENABLED:
                SHELL_LOG_USER_ERROR("Volume not enabled");
                break;
            case FR_NO_FILESYSTEM:
                SHELL_LOG_USER_ERROR("No valid filesystem found on SD card");
                break;
            default:
                SHELL_LOG_USER_ERROR("Unknown error code: %d", res);
                break;
        }
        
        SHELL_LOG_USER_INFO("Attempting to unmount and remount SD card...");
        
        // First try to unmount
        f_mount(NULL, USERPath, 0);
        HAL_Delay(100);
        
        // Try to mount with force flag
        res = f_mount(&USERFatFS, USERPath, 1);
        if (res != FR_OK) {
            SHELL_LOG_USER_ERROR("Failed to mount SD card, FRESULT: %d", res);
            
            // Print detailed mount error information
            switch(res) {
                case FR_NOT_READY:
                    SHELL_LOG_USER_ERROR("Mount failed: Drive not ready");
                    break;
                case FR_DISK_ERR:
                    SHELL_LOG_USER_ERROR("Mount failed: Disk error");
                    break;
                case FR_NOT_ENABLED:
                    SHELL_LOG_USER_ERROR("Mount failed: Volume not enabled");
                    break;
                case FR_NO_FILESYSTEM:
                    SHELL_LOG_USER_ERROR("Mount failed: No filesystem");
                    break;
                case FR_INVALID_DRIVE:
                    SHELL_LOG_USER_ERROR("Mount failed: Invalid drive");
                    break;
                default:
                    SHELL_LOG_USER_ERROR("Mount failed: Unknown error %d", res);
                    break;
            }
            
            recorder.state = AUDIO_REC_ERROR;
            return -1;
        }
        SHELL_LOG_USER_INFO("SD card mounted successfully");
        
        // Verify mount by checking free space again
        HAL_Delay(50);
        res = f_getfree("0:", &fre_clust, &fs);
        if (res != FR_OK) {
            SHELL_LOG_USER_ERROR("SD card mount verification failed, FRESULT: %d", res);
            recorder.state = AUDIO_REC_ERROR;
            return -1;
        }
        SHELL_LOG_USER_INFO("SD card mount verified, free clusters: %lu", fre_clust);
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
    SHELL_LOG_USER_INFO("f_open result: %d for path: %s", res, full_path);
    
    if (res == FR_OK) {
        // 立即同步文件以确保创建
        FRESULT sync_res = f_sync(&recorder.file);
        SHELL_LOG_USER_INFO("f_sync after create result: %d", sync_res);
        
        // 简化：不再写入测试数据，避免扰乱实际PCM文件内容
    }
    
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
    
    // 验证文件是否真的创建成功 - 通过获取文件大小而不是重新打开
    FSIZE_t current_size = f_size(&recorder.file);
    SHELL_LOG_USER_INFO("File created successfully, current size: %lu bytes", (unsigned long)current_size);
    
    // 记录实际使用的文件路径
    const char* actual_path = (res == FR_OK && strstr(full_path, "audio/")) ? full_path : recorder.filename;
    SHELL_LOG_USER_INFO("File path being used: %s", actual_path);
    
    // 确保状态正确设置为录音中
    recorder.state = AUDIO_REC_RECORDING;
    SHELL_LOG_USER_DEBUG("State set to RECORDING: %d", recorder.state);
    
    // Start SAI DMA reception
    SHELL_LOG_USER_INFO("Starting SAI DMA reception, buffer size: %d", AUDIO_BUFFER_SIZE / 2);
    SHELL_LOG_USER_DEBUG("SAI handle: %p, buffer address: %p", &hsai_BlockA4, audio_buffer);
    SHELL_LOG_USER_DEBUG("SAI state before start: %d", HAL_SAI_GetState(&hsai_BlockA4));

    /* Buffer is non-cacheable via MPU; no cache maintenance needed */
    
    HAL_StatusTypeDef sai_result = HAL_SAI_Receive_DMA(&hsai_BlockA4, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE / 2);
    SHELL_LOG_USER_DEBUG("SAI_Receive_DMA result: %d", sai_result);
    
    if (sai_result != HAL_OK) {
        SHELL_LOG_USER_ERROR("Failed to start SAI DMA reception, HAL result: %d", sai_result);
        SHELL_LOG_USER_DEBUG("SAI state after failed start: %d", HAL_SAI_GetState(&hsai_BlockA4));
        f_close(&recorder.file);
        recorder.file_open = false;
        recorder.state = AUDIO_REC_ERROR;
        SHELL_LOG_USER_DEBUG("State set to ERROR due to SAI failure: %d", recorder.state);
        return -1;
    }
    
    SHELL_LOG_USER_DEBUG("SAI DMA started successfully");
    SHELL_LOG_USER_DEBUG("SAI state after successful start: %d", HAL_SAI_GetState(&hsai_BlockA4));
    
    // Optional status check (enable by defining AUDIO_REC_VERBOSE)
    HAL_Delay(20);
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
        SHELL_LOG_USER_WARNING("No active recording to stop (already in idle state)");
        return 0; // 不是错误，只是没有录音在进行
    }
    
    if (recorder.state == AUDIO_REC_STOPPING) {
        SHELL_LOG_USER_WARNING("Recording is already stopping");
        return 0; // 已经在停止过程中
    }
    
    if (recorder.state != AUDIO_REC_RECORDING && recorder.state != AUDIO_REC_ERROR) {
        SHELL_LOG_USER_ERROR("Cannot stop recording, unexpected state: %d", recorder.state);
        return -1;
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
        HAL_Delay(10);
        
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
        HAL_Delay(1);
    }
    
    if (HAL_SAI_GetState(&hsai_BlockA4) != HAL_SAI_STATE_READY) {
        SHELL_LOG_USER_WARNING("SAI not in ready state after stop, current state: %d", HAL_SAI_GetState(&hsai_BlockA4));
    } else {
        SHELL_LOG_USER_DEBUG("SAI is now in ready state");
    }
    
    // Close file with enhanced error handling
    if (recorder.file_open) {
        SHELL_LOG_USER_INFO("Syncing and closing file...");
        
        // First sync the file to ensure all data is written
        FRESULT sync_result = f_sync(&recorder.file);
        if (sync_result != FR_OK) {
            SHELL_LOG_USER_WARNING("File sync failed, FRESULT: %d", sync_result);
        } else {
            SHELL_LOG_USER_DEBUG("File synced successfully");
        }
        
        // Wait a bit to ensure sync is complete
        HAL_Delay(10);
        
        // Attempt to close file with retry
        FRESULT close_result = FR_DISK_ERR;
        int close_attempts = 0;
        const int max_close_attempts = 3;
        
        while (close_attempts < max_close_attempts && close_result != FR_OK) {
            close_attempts++;
            SHELL_LOG_USER_DEBUG("File close attempt %d/%d", close_attempts, max_close_attempts);
            
            close_result = f_close(&recorder.file);
            
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
                    HAL_Delay(50); // Wait before retry
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
        
        // 强制文件系统同步，确保所有缓存数据写入SD卡
        SHELL_LOG_USER_INFO("Forcing filesystem sync...");
        FRESULT sync_fs_result = f_sync(NULL); // 同步整个文件系统
        SHELL_LOG_USER_INFO("Filesystem sync result: %d", sync_fs_result);
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
    
    // Always check SD card status
    check_sd_card_status();
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        debug_sai_status();
        
        // 检查当前缓冲区内容
        int current_non_zero = 0;
        for (int i = 0; i < 10 && i < AUDIO_BUFFER_SIZE / 4; i++) {
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
 * @brief Audio processing thread function
 * @param argument: Not used
 */
static void audio_process_thread(void *argument)
{
    (void)argument;
    for (;;) {
        // Wait for a signal from the DMA callbacks
        if (osSemaphoreAcquire(audio_data_sem, osWaitForever) == osOK) {
            // Call the processing function if recording
            if (recorder.state == AUDIO_REC_RECORDING) {
                audio_recorder_process();
            }
        }
    }
}

/**
 * @brief Process audio data buffers. This should be called from the main loop.
 */
void audio_recorder_process(void)
{
    if (recorder.state != AUDIO_REC_RECORDING) {
        return;
    }

    if (half_buffer_ready) {
        half_buffer_ready = false;
    // Non-cacheable region: no invalidate required
        if (write_audio_data(&audio_buffer[0], AUDIO_BUFFER_SIZE / 2) != 0) {
            SHELL_LOG_USER_ERROR("Failed to write first half of buffer, stopping recording.");
            audio_recorder_stop();
            recorder.state = AUDIO_REC_ERROR;
        }
    }

    if (buffer_ready) {
        buffer_ready = false;
    // Non-cacheable region: no invalidate required
        if (write_audio_data(&audio_buffer[AUDIO_BUFFER_SIZE / 4], AUDIO_BUFFER_SIZE / 2) != 0) {
            SHELL_LOG_USER_ERROR("Failed to write second half of buffer, stopping recording.");
            audio_recorder_stop();
            recorder.state = AUDIO_REC_ERROR;
        }
    }
}

/* Callback functions --------------------------------------------------------*/

/**
 * @brief SAI RX complete callback
 */
void audio_recorder_rx_complete_callback(void)
{
    /* Keep callbacks lightweight: no debug log unless verbose */
    SHELL_LOG_USER_DEBUG("SAI RX complete");
    if (recorder.state == AUDIO_REC_RECORDING) {
        buffer_ready = true;
        osSemaphoreRelease(audio_data_sem);
    }
}

/**
 * @brief SAI RX half complete callback
 */
void audio_recorder_rx_half_complete_callback(void)
{
    /* Half complete */
    SHELL_LOG_USER_DEBUG("SAI RX half");
    if (recorder.state == AUDIO_REC_RECORDING) {
        half_buffer_ready = true;
        osSemaphoreRelease(audio_data_sem);
    }
}

/**
 * @brief SAI error callback
 */
void audio_recorder_error_callback(void)
{
    SHELL_LOG_USER_ERROR("SAI error callback (state=%d)", recorder.state);
    
    if (recorder.state == AUDIO_REC_RECORDING) {
        SHELL_LOG_USER_ERROR("SAI error during recording, stopping...");
        
        // Print detailed status at the moment of error
        debug_sai_status();
        
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