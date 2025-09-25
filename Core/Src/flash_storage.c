/**
 * @file flash_storage.c
 * @brief External Flash storage management for audio data buffering using W25Q128JV
 * @date 2025-01-28
 */

/* Includes ------------------------------------------------------------------*/
#include "../Inc/flash_storage.h"
#include "octospi.h"
#include "shell_log.h"
#include "fatfs.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "cmsis_os.h"

/* Private define ------------------------------------------------------------*/
#define FLASH_TIMEOUT_MS              1000   // 1 second timeout for flash operations
#define FLASH_WRITE_CHUNK_SIZE        256    // Write page size (256 bytes)
#define FLASH_READ_CHUNK_SIZE         4096   // Read in 4KB chunks for efficiency
#define FLASH_ERASE_SECTOR_SIZE       4096   // 4KB sector erase
#define FLASH_MAX_ERASE_TIME_MS       400    // Max sector erase time
#define FLASH_MAX_PAGE_PROGRAM_TIME_MS 3     // Max page program time

/* W25Q128JV Specific Commands */
#define W25Q128_CMD_WRITE_ENABLE       0x06
#define W25Q128_CMD_WRITE_DISABLE      0x04
#define W25Q128_CMD_READ_STATUS_REG1   0x05
#define W25Q128_CMD_READ_STATUS_REG2   0x35
#define W25Q128_CMD_WRITE_STATUS_REG   0x01
#define W25Q128_CMD_PAGE_PROGRAM       0x02
#define W25Q128_CMD_QUAD_PAGE_PROGRAM  0x32
#define W25Q128_CMD_SECTOR_ERASE       0x20
#define W25Q128_CMD_BLOCK_ERASE_32K    0x52
#define W25Q128_CMD_BLOCK_ERASE_64K    0xD8
#define W25Q128_CMD_CHIP_ERASE         0xC7
#define W25Q128_CMD_READ_DATA          0x03
#define W25Q128_CMD_FAST_READ          0x0B
#define W25Q128_CMD_QUAD_READ          0xEB
#define W25Q128_CMD_READ_JEDEC_ID      0x9F
#define W25Q128_CMD_READ_UNIQUE_ID     0x4B
#define W25Q128_CMD_POWER_DOWN         0xB9
#define W25Q128_CMD_RELEASE_POWER_DOWN 0xAB

/* Status Register Bits */
#define W25Q128_STATUS_BUSY            0x01
#define W25Q128_STATUS_WEL             0x02

/* Private variables ---------------------------------------------------------*/
static FlashStorageManager_t flash_manager = {0};
static osMutexId_t flash_mutex = NULL;

/* Private function prototypes -----------------------------------------------*/
static FlashStorageStatus_t flash_send_command(uint8_t cmd);
static FlashStorageStatus_t flash_write_enable(void);
static FlashStorageStatus_t flash_wait_for_ready(uint32_t timeout_ms);
static FlashStorageStatus_t flash_read_status_register(uint8_t* status);
static FlashStorageStatus_t flash_read_jedec_id(uint8_t* id_buffer);
static FlashStorageStatus_t flash_sector_erase(uint32_t address);
static FlashStorageStatus_t flash_page_program(uint32_t address, const uint8_t* data, uint32_t size);
static FlashStorageStatus_t flash_read_data(uint32_t address, uint8_t* data, uint32_t size);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize external flash storage
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_init(void)
{
    uint8_t jedec_id[3] = {0};
    uint32_t init_start_time = HAL_GetTick();
    const uint32_t INIT_TIMEOUT_MS = 5000;  // 5 second timeout for entire init
    
    SHELL_LOG_USER_INFO("Initializing W25Q128JV Flash storage...");
    SHELL_LOG_USER_DEBUG("Step 1: Creating flash mutex...");
    
    // Create mutex for thread safety
    if (flash_mutex == NULL) {
        flash_mutex = osMutexNew(NULL);
        if (flash_mutex == NULL) {
            SHELL_LOG_USER_ERROR("Failed to create flash mutex");
            return FLASH_STORAGE_ERROR;
        }
    }
    SHELL_LOG_USER_DEBUG("Flash mutex created successfully");
    
    // Lock mutex
    SHELL_LOG_USER_DEBUG("Step 2: Acquiring flash mutex...");
    if (osMutexAcquire(flash_mutex, FLASH_TIMEOUT_MS) != osOK) {
        SHELL_LOG_USER_ERROR("Failed to acquire flash mutex");
        return FLASH_STORAGE_TIMEOUT;
    }
    SHELL_LOG_USER_DEBUG("Flash mutex acquired successfully");
    
    // Initialize OCTOSPI2 for W25Q128JV
    SHELL_LOG_USER_DEBUG("Step 3: Initializing OCTOSPI2...");
    MX_OCTOSPI2_Init();
    
    // Check timeout after each major step
    if ((HAL_GetTick() - init_start_time) > INIT_TIMEOUT_MS) {
        SHELL_LOG_USER_ERROR("Flash init timeout at step 3");
        osMutexRelease(flash_mutex);
        return FLASH_STORAGE_TIMEOUT;
    }
    
    // Add delay for stabilization
    SHELL_LOG_USER_DEBUG("Step 4: OCTOSPI2 initialized, skipping delay for debug...");
    // vTaskDelay(pdMS_TO_TICKS(10));  // Temporarily disabled for debugging
    SHELL_LOG_USER_DEBUG("OCTOSPI2 stabilization complete");
    
    // Check timeout
    if ((HAL_GetTick() - init_start_time) > INIT_TIMEOUT_MS) {
        SHELL_LOG_USER_ERROR("Flash init timeout at step 4");
        osMutexRelease(flash_mutex);
        return FLASH_STORAGE_TIMEOUT;
    }
    
    // Try to read status register first (simpler command)
    SHELL_LOG_USER_DEBUG("Step 5: Testing Flash communication with status register read...");
    uint8_t status_reg = 0;
    FlashStorageStatus_t flash_status = flash_read_status_register(&status_reg);
    if (flash_status == FLASH_STORAGE_OK) {
        SHELL_LOG_USER_DEBUG("Step 5 SUCCESS: Flash status register: 0x%02X", status_reg);
    } else {
        SHELL_LOG_USER_WARNING("Step 5 FAILED: Failed to read Flash status register, trying reset sequence...");
        
        // Try reset sequence
        SHELL_LOG_USER_DEBUG("Step 5a: Sending power-down release command...");
        flash_send_command(W25Q128_CMD_RELEASE_POWER_DOWN);
        vTaskDelay(pdMS_TO_TICKS(1));
        SHELL_LOG_USER_DEBUG("Step 5b: Sending write enable command...");
        flash_send_command(W25Q128_CMD_WRITE_ENABLE);
        vTaskDelay(pdMS_TO_TICKS(1));
        SHELL_LOG_USER_DEBUG("Reset sequence complete");
    }
    
    // Reset the flash chip by sending power cycle commands
    SHELL_LOG_USER_DEBUG("Step 6: Sending power-down release command...");
    FlashStorageStatus_t status = flash_send_command(W25Q128_CMD_RELEASE_POWER_DOWN);
    if (status != FLASH_STORAGE_OK) {
        SHELL_LOG_USER_WARNING("Step 6 WARNING: Flash power-down release command failed, continuing...");
    } else {
        SHELL_LOG_USER_DEBUG("Step 6 SUCCESS: Power-down release command completed");
    }
    
    // Wait for chip to be ready
    SHELL_LOG_USER_DEBUG("Step 7: Waiting for chip power-up...");
    vTaskDelay(2); // Increased delay for power-up
    SHELL_LOG_USER_DEBUG("Step 7 complete: Power-up delay finished");
    
    // Read JEDEC ID to verify chip presence and type
    SHELL_LOG_USER_DEBUG("Step 8: Attempting to read JEDEC ID...");
    
    // Check timeout before JEDEC ID read
    if ((HAL_GetTick() - init_start_time) > INIT_TIMEOUT_MS) {
        SHELL_LOG_USER_ERROR("Flash init timeout before JEDEC ID read");
        osMutexRelease(flash_mutex);
        return FLASH_STORAGE_TIMEOUT;
    }
    
    status = flash_read_jedec_id(jedec_id);
    if (status != FLASH_STORAGE_OK) {
        if (jedec_id[0] == 0x00 && jedec_id[1] == 0x00 && jedec_id[2] == 0x00) {
            SHELL_LOG_USER_ERROR("All-00 response suggests no SPI communication");
            SHELL_LOG_USER_ERROR("Check OCTOSPI2 CLK and CS pins");
        } else if (jedec_id[0] == 0xFF && jedec_id[1] == 0xFF && jedec_id[2] == 0xFF) {
            SHELL_LOG_USER_ERROR("All-FF response suggests MISO line stuck high");
            SHELL_LOG_USER_ERROR("Check W25Q128JV power (VCC should be 3.3V)");
            SHELL_LOG_USER_ERROR("Check WP# and HOLD# pins (should be pulled to VCC)");
        }
        SHELL_LOG_USER_ERROR("Failed to read Flash JEDEC ID, HAL status might indicate communication issue");
        
        // Try alternative approach: send simple command
        SHELL_LOG_USER_DEBUG("Trying alternative communication test...");
        uint8_t test_status = 0;
        if (flash_read_status_register(&test_status) == FLASH_STORAGE_OK) {
            SHELL_LOG_USER_DEBUG("Status register read successful: 0x%02X", test_status);
        } else {
            SHELL_LOG_USER_ERROR("All Flash communication attempts failed");
            SHELL_LOG_USER_ERROR("Hardware troubleshooting guide:");
            SHELL_LOG_USER_ERROR("1. Verify OCTOSPI2 pin connections:");
            SHELL_LOG_USER_ERROR("   - PF4 -> W25Q128JV CLK");
            SHELL_LOG_USER_ERROR("   - PG12 -> W25Q128JV CS#");
            SHELL_LOG_USER_ERROR("   - PG0 -> W25Q128JV DI (MOSI)");
            SHELL_LOG_USER_ERROR("   - PG1 -> W25Q128JV DO (MISO)");
            SHELL_LOG_USER_ERROR("2. Check power connections: VCC=3.3V, GND");
            SHELL_LOG_USER_ERROR("3. Pull WP# and HOLD# high to VCC");
            SHELL_LOG_USER_ERROR("4. Use audio_flash_test command for more diagnostics");
        }
        
        osMutexRelease(flash_mutex);
        return FLASH_STORAGE_ERROR;
    }
    
    SHELL_LOG_USER_DEBUG("JEDEC ID read successful: 0x%02X 0x%02X 0x%02X", 
                        jedec_id[0], jedec_id[1], jedec_id[2]);
    
    // Verify JEDEC ID for W25Q128JV (expected: 0xEF, 0x40, 0x18)
    if (jedec_id[0] != 0xEF || jedec_id[1] != 0x40 || jedec_id[2] != 0x18) {
        SHELL_LOG_USER_ERROR("Unexpected JEDEC ID: 0x%02X 0x%02X 0x%02X (expected: 0xEF 0x40 0x18)",
                            jedec_id[0], jedec_id[1], jedec_id[2]);
        
        // Additional diagnostics for 0xFF values
        if (jedec_id[0] == 0xFF && jedec_id[1] == 0xFF && jedec_id[2] == 0xFF) {
            SHELL_LOG_USER_ERROR("All-FF response suggests flash chip not responding");
            SHELL_LOG_USER_ERROR("Possible issues:");
            SHELL_LOG_USER_ERROR("1. Flash chip not powered");
            SHELL_LOG_USER_ERROR("2. OCTOSPI2 pins not connected to flash");
            SHELL_LOG_USER_ERROR("3. Flash chip in deep power-down mode");
            SHELL_LOG_USER_ERROR("4. Wrong chip or faulty chip");
            
            // Try multiple wake-up sequences
            SHELL_LOG_USER_DEBUG("Trying extended wake-up sequence...");
            for (int i = 0; i < 3; i++) {
                flash_send_command(W25Q128_CMD_RELEASE_POWER_DOWN);
                vTaskDelay(pdMS_TO_TICKS(10));
                flash_send_command(W25Q128_CMD_WRITE_ENABLE);
                vTaskDelay(pdMS_TO_TICKS(1));
                
                // Try reading JEDEC ID again
                if (flash_read_jedec_id(jedec_id) == FLASH_STORAGE_OK) {
                    SHELL_LOG_USER_DEBUG("Retry %d JEDEC ID: 0x%02X 0x%02X 0x%02X", 
                                        i+1, jedec_id[0], jedec_id[1], jedec_id[2]);
                    if (jedec_id[0] == 0xEF && jedec_id[1] == 0x40 && jedec_id[2] == 0x18) {
                        SHELL_LOG_USER_INFO("Flash responded after %d retries!", i+1);
                        break;
                    }
                }
            }
        } else if (jedec_id[0] == 0x00 && jedec_id[1] == 0x00 && jedec_id[2] == 0x00) {
            SHELL_LOG_USER_ERROR("All-00 response suggests no SPI communication");
            SHELL_LOG_USER_ERROR("Check OCTOSPI2 CLK and CS pins");
        }
        
        // Check final result after retries
        if (jedec_id[0] != 0xEF || jedec_id[1] != 0x40 || jedec_id[2] != 0x18) {
            osMutexRelease(flash_mutex);
            return FLASH_STORAGE_ERROR;
        }
    }
    
    SHELL_LOG_USER_INFO("W25Q128JV Flash detected, JEDEC ID: 0x%02X 0x%02X 0x%02X",
                       jedec_id[0], jedec_id[1], jedec_id[2]);
    
    // Initialize flash manager structure
    flash_manager.write_address = FLASH_AUDIO_START_ADDRESS;
    flash_manager.read_address = FLASH_AUDIO_START_ADDRESS;
    flash_manager.total_bytes_written = 0;
    flash_manager.available_space = FLASH_TOTAL_SIZE;
    flash_manager.is_initialized = true;
    flash_manager.is_full = false;
    
    osMutexRelease(flash_mutex);
    
    SHELL_LOG_USER_INFO("Flash storage initialized successfully");
    SHELL_LOG_USER_INFO("Total capacity: %d MB (%d bytes)", 
                       FLASH_TOTAL_SIZE / (1024 * 1024), FLASH_TOTAL_SIZE);
    
    return FLASH_STORAGE_OK;
}

/**
 * @brief Deinitialize external flash storage
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_deinit(void)
{
    if (!flash_manager.is_initialized) {
        return FLASH_STORAGE_OK;
    }
    
    // Lock mutex
    if (osMutexAcquire(flash_mutex, FLASH_TIMEOUT_MS) != osOK) {
        return FLASH_STORAGE_TIMEOUT;
    }
    
    // Send power down command to reduce power consumption
    flash_send_command(W25Q128_CMD_POWER_DOWN);
    
    // Clear manager structure
    memset(&flash_manager, 0, sizeof(flash_manager));
    
    osMutexRelease(flash_mutex);
    
    SHELL_LOG_USER_INFO("Flash storage deinitialized");
    return FLASH_STORAGE_OK;
}

/**
 * @brief Erase all audio data in flash (prepare for new recording)
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_erase_audio_area(void)
{
    if (!flash_manager.is_initialized) {
        SHELL_LOG_USER_ERROR("Flash not initialized");
        return FLASH_STORAGE_ERROR;
    }
    
    SHELL_LOG_USER_INFO("Erasing flash audio area (16MB)...");
    
    // Lock mutex
    if (osMutexAcquire(flash_mutex, FLASH_TIMEOUT_MS) != osOK) {
        return FLASH_STORAGE_TIMEOUT;
    }
    
    uint32_t start_time = HAL_GetTick();
    uint32_t sectors_erased = 0;
    uint32_t total_sectors = FLASH_TOTAL_SIZE / FLASH_ERASE_SECTOR_SIZE;
    
    // Erase flash in 4KB sectors
    for (uint32_t address = FLASH_AUDIO_START_ADDRESS; 
         address < FLASH_AUDIO_END_ADDRESS; 
         address += FLASH_ERASE_SECTOR_SIZE) {
        
        FlashStorageStatus_t status = flash_sector_erase(address);
        if (status != FLASH_STORAGE_OK) {
            SHELL_LOG_USER_ERROR("Failed to erase sector at address 0x%08lX", address);
            osMutexRelease(flash_mutex);
            return status;
        }
        
        sectors_erased++;
        
        // Log progress every 256 sectors (1MB)
        if (sectors_erased % 256 == 0) {
            uint32_t percent = (sectors_erased * 100) / total_sectors;
            SHELL_LOG_USER_INFO("Erase progress: %ld%% (%ld/%ld sectors)", 
                               percent, sectors_erased, total_sectors);
        }
    }
    
    // Reset manager counters
    flash_manager.write_address = FLASH_AUDIO_START_ADDRESS;
    flash_manager.read_address = FLASH_AUDIO_START_ADDRESS;
    flash_manager.total_bytes_written = 0;
    flash_manager.available_space = FLASH_TOTAL_SIZE;
    flash_manager.is_full = false;
    
    uint32_t elapsed_time = HAL_GetTick() - start_time;
    
    osMutexRelease(flash_mutex);
    
    SHELL_LOG_USER_INFO("Flash erase completed in %ld ms", elapsed_time);
    return FLASH_STORAGE_OK;
}

/**
 * @brief Write audio data to flash
 * @param data: Pointer to audio data
 * @param size: Size of data in bytes
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_write_audio_data(const uint8_t* data, uint32_t size)
{
    if (!flash_manager.is_initialized) {
        return FLASH_STORAGE_ERROR;
    }
    
    if (data == NULL || size == 0) {
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    // Check if flash is full
    if (flash_manager.is_full || 
        (flash_manager.write_address + size) > FLASH_AUDIO_END_ADDRESS) {
        flash_manager.is_full = true;
        return FLASH_STORAGE_FULL;
    }
    
    // Lock mutex
    if (osMutexAcquire(flash_mutex, FLASH_TIMEOUT_MS) != osOK) {
        return FLASH_STORAGE_TIMEOUT;
    }
    
    FlashStorageStatus_t overall_status = FLASH_STORAGE_OK;
    uint32_t bytes_written = 0;
    uint32_t current_address = flash_manager.write_address;
    
    while (bytes_written < size) {
        // Calculate how much to write in this page
        uint32_t bytes_to_write = size - bytes_written;
        uint32_t page_offset = current_address % FLASH_PAGE_SIZE;
        uint32_t page_remaining = FLASH_PAGE_SIZE - page_offset;
        
        if (bytes_to_write > page_remaining) {
            bytes_to_write = page_remaining;
        }
        
        // Write to current page
        FlashStorageStatus_t status = flash_page_program(current_address, 
                                                        &data[bytes_written], 
                                                        bytes_to_write);
        if (status != FLASH_STORAGE_OK) {
            SHELL_LOG_USER_ERROR("Failed to write page at address 0x%08lX", current_address);
            overall_status = status;
            break;
        }
        
        bytes_written += bytes_to_write;
        current_address += bytes_to_write;
    }
    
    if (overall_status == FLASH_STORAGE_OK) {
        // Update manager counters
        flash_manager.write_address = current_address;
        flash_manager.total_bytes_written += bytes_written;
        flash_manager.available_space = FLASH_AUDIO_END_ADDRESS - flash_manager.write_address;
        
        // Check if flash is now full
        if (flash_manager.available_space == 0) {
            flash_manager.is_full = true;
            SHELL_LOG_USER_WARNING("Flash storage is now full!");
        }
    }
    
    osMutexRelease(flash_mutex);
    return overall_status;
}

/**
 * @brief Read audio data from flash
 * @param data: Pointer to buffer for read data
 * @param size: Size of data to read
 * @param address: Address in flash to read from (relative to audio start)
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_read_audio_data(uint8_t* data, uint32_t size, uint32_t address)
{
    if (!flash_manager.is_initialized) {
        return FLASH_STORAGE_ERROR;
    }
    
    if (data == NULL || size == 0) {
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    uint32_t absolute_address = FLASH_AUDIO_START_ADDRESS + address;
    
    // Check bounds
    if ((absolute_address + size) > FLASH_AUDIO_END_ADDRESS) {
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    // Lock mutex
    if (osMutexAcquire(flash_mutex, FLASH_TIMEOUT_MS) != osOK) {
        return FLASH_STORAGE_TIMEOUT;
    }
    
    FlashStorageStatus_t status = flash_read_data(absolute_address, data, size);
    
    osMutexRelease(flash_mutex);
    return status;
}

/**
 * @brief Copy all audio data from flash to TF card file
 * @param filename: Target filename on TF card
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_copy_to_tf_card(const char* filename)
{
    if (!flash_manager.is_initialized) {
        SHELL_LOG_USER_ERROR("Flash not initialized");
        return FLASH_STORAGE_ERROR;
    }
    
    if (filename == NULL) {
        SHELL_LOG_USER_ERROR("Invalid filename");
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    if (flash_manager.total_bytes_written == 0) {
        SHELL_LOG_USER_WARNING("No audio data in flash to copy");
        return FLASH_STORAGE_OK;
    }
    
    SHELL_LOG_USER_INFO("Copying %ld bytes from flash to TF card file: %s", 
                       flash_manager.total_bytes_written, filename);
    
    FIL tf_file;
    FRESULT res;
    uint8_t* buffer = NULL;
    FlashStorageStatus_t overall_status = FLASH_STORAGE_OK;
    
    // Allocate read buffer
    buffer = pvPortMalloc(FLASH_READ_CHUNK_SIZE);
    if (buffer == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate read buffer");
        return FLASH_STORAGE_ERROR;
    }
    
    // Open TF card file for writing
    res = f_open(&tf_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to open TF card file: %s, error: %d", filename, res);
        vPortFree(buffer);
        return FLASH_STORAGE_ERROR;
    }
    
    uint32_t bytes_copied = 0;
    uint32_t start_time = HAL_GetTick();
    
    while (bytes_copied < flash_manager.total_bytes_written) {
        uint32_t chunk_size = flash_manager.total_bytes_written - bytes_copied;
        if (chunk_size > FLASH_READ_CHUNK_SIZE) {
            chunk_size = FLASH_READ_CHUNK_SIZE;
        }
        
        // Read from flash
        FlashStorageStatus_t flash_status = flash_storage_read_audio_data(buffer, chunk_size, bytes_copied);
        if (flash_status != FLASH_STORAGE_OK) {
            SHELL_LOG_USER_ERROR("Failed to read from flash at offset %ld", bytes_copied);
            overall_status = flash_status;
            break;
        }
        
        // Write to TF card
        UINT bytes_written;
        res = f_write(&tf_file, buffer, chunk_size, &bytes_written);
        if (res != FR_OK || bytes_written != chunk_size) {
            SHELL_LOG_USER_ERROR("Failed to write to TF card, error: %d", res);
            overall_status = FLASH_STORAGE_ERROR;
            break;
        }
        
        bytes_copied += chunk_size;
        
        // Log progress every 1MB
        if (bytes_copied % (1024 * 1024) == 0) {
            uint32_t percent = (bytes_copied * 100) / flash_manager.total_bytes_written;
            SHELL_LOG_USER_INFO("Copy progress: %ld%% (%ld/%ld bytes)", 
                               percent, bytes_copied, flash_manager.total_bytes_written);
        }
    }
    
    // Sync and close file
    f_sync(&tf_file);
    f_close(&tf_file);
    
    uint32_t elapsed_time = HAL_GetTick() - start_time;
    
    if (overall_status == FLASH_STORAGE_OK) {
        SHELL_LOG_USER_INFO("Successfully copied %ld bytes to %s in %ld ms", 
                           bytes_copied, filename, elapsed_time);
        
        // Calculate transfer speed
        if (elapsed_time > 0) {
            uint32_t speed_kbps = (bytes_copied / elapsed_time);
            SHELL_LOG_USER_INFO("Transfer speed: %ld KB/s", speed_kbps);
        }
    } else {
        SHELL_LOG_USER_ERROR("Copy failed after %ld bytes", bytes_copied);
    }
    
    vPortFree(buffer);
    return overall_status;
}

/**
 * @brief Check if flash storage is full
 * @retval bool True if full, false otherwise
 */
bool flash_storage_is_full(void)
{
    return flash_manager.is_full;
}

/**
 * @brief Get current storage information
 * @param manager: Pointer to storage manager structure to fill
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_get_info(FlashStorageManager_t* manager)
{
    if (manager == NULL) {
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    if (!flash_manager.is_initialized) {
        return FLASH_STORAGE_ERROR;
    }
    
    *manager = flash_manager;
    return FLASH_STORAGE_OK;
}

/**
 * @brief Get remaining available space in bytes
 * @retval uint32_t Available space in bytes
 */
uint32_t flash_storage_get_available_space(void)
{
    return flash_manager.available_space;
}

/**
 * @brief Get total bytes written to flash
 * @retval uint32_t Total bytes written
 */
uint32_t flash_storage_get_total_written(void)
{
    return flash_manager.total_bytes_written;
}

/**
 * @brief Test flash storage functionality
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_test(void)
{
    if (!flash_manager.is_initialized) {
        SHELL_LOG_USER_ERROR("Flash not initialized");
        return FLASH_STORAGE_ERROR;
    }
    
    SHELL_LOG_USER_INFO("Testing flash storage functionality...");
    
    const uint32_t test_data_size = 1024;
    uint8_t* write_buffer = pvPortMalloc(test_data_size);
    uint8_t* read_buffer = pvPortMalloc(test_data_size);
    
    if (write_buffer == NULL || read_buffer == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate test buffers");
        if (write_buffer) vPortFree(write_buffer);
        if (read_buffer) vPortFree(read_buffer);
        return FLASH_STORAGE_ERROR;
    }
    
    // Fill write buffer with test pattern
    for (uint32_t i = 0; i < test_data_size; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    FlashStorageStatus_t status = FLASH_STORAGE_OK;
    
    // Test sector erase
    status = flash_sector_erase(FLASH_AUDIO_START_ADDRESS);
    if (status != FLASH_STORAGE_OK) {
        SHELL_LOG_USER_ERROR("Flash erase test failed");
        goto cleanup;
    }
    
    // Test write
    status = flash_storage_write_audio_data(write_buffer, test_data_size);
    if (status != FLASH_STORAGE_OK) {
        SHELL_LOG_USER_ERROR("Flash write test failed");
        goto cleanup;
    }
    
    // Test read
    status = flash_storage_read_audio_data(read_buffer, test_data_size, 0);
    if (status != FLASH_STORAGE_OK) {
        SHELL_LOG_USER_ERROR("Flash read test failed");
        goto cleanup;
    }
    
    // Verify data
    if (memcmp(write_buffer, read_buffer, test_data_size) != 0) {
        SHELL_LOG_USER_ERROR("Flash data verification failed");
        status = FLASH_STORAGE_ERROR;
        goto cleanup;
    }
    
    SHELL_LOG_USER_INFO("Flash storage test passed");
    
cleanup:
    vPortFree(write_buffer);
    vPortFree(read_buffer);
    return status;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Send a simple command to flash
 * @param cmd: Command to send
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_send_command(uint8_t cmd)
{
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    SHELL_LOG_USER_DEBUG("Sending flash command 0x%02X", cmd);
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = cmd;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_NONE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_NONE;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    SHELL_LOG_USER_DEBUG("Calling HAL_OSPI_Command for cmd 0x%02X", cmd);
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    SHELL_LOG_USER_DEBUG("HAL_OSPI_Command returned: %d for cmd 0x%02X", hal_status, cmd);
    
    return (hal_status == HAL_OK) ? FLASH_STORAGE_OK : FLASH_STORAGE_ERROR;
}

/**
 * @brief Enable write operations on flash
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_write_enable(void)
{
    return flash_send_command(W25Q128_CMD_WRITE_ENABLE);
}

/**
 * @brief Wait for flash to be ready (not busy)
 * @param timeout_ms: Timeout in milliseconds
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_wait_for_ready(uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    uint8_t status;
    
    do {
        FlashStorageStatus_t result = flash_read_status_register(&status);
        if (result != FLASH_STORAGE_OK) {
            return result;
        }
        
        if ((status & W25Q128_STATUS_BUSY) == 0) {
            return FLASH_STORAGE_OK;
        }
        
        vTaskDelay(1); // Yield to other tasks
        
    } while ((HAL_GetTick() - start_time) < timeout_ms);
    
    return FLASH_STORAGE_TIMEOUT;
}

/**
 * @brief Read status register from flash
 * @param status: Pointer to store status byte
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_read_status_register(uint8_t* status)
{
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    SHELL_LOG_USER_DEBUG("Reading flash status register...");
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = W25Q128_CMD_READ_STATUS_REG1;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_NONE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_1_LINE;  // Start with SPI mode
    cmd_cfg.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
    cmd_cfg.NbData = 1;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    SHELL_LOG_USER_DEBUG("Calling HAL_OSPI_Command for status read...");
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    SHELL_LOG_USER_DEBUG("HAL_OSPI_Command returned: %d", hal_status);
    if (hal_status != HAL_OK) {
        return FLASH_STORAGE_ERROR;
    }
    
    SHELL_LOG_USER_DEBUG("Calling HAL_OSPI_Receive for status read...");
    hal_status = HAL_OSPI_Receive(&hospi2, status, FLASH_TIMEOUT_MS);
    SHELL_LOG_USER_DEBUG("HAL_OSPI_Receive returned: %d, status: 0x%02X", hal_status, *status);
    
    return (hal_status == HAL_OK) ? FLASH_STORAGE_OK : FLASH_STORAGE_ERROR;
}

/**
 * @brief Read JEDEC ID from flash
 * @param id_buffer: Buffer to store 3-byte JEDEC ID
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_read_jedec_id(uint8_t* id_buffer)
{
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    SHELL_LOG_USER_DEBUG("Reading JEDEC ID with QSPI quad read command...");
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = W25Q128_CMD_READ_JEDEC_ID;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_NONE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_4_LINES;  // Try QSPI 4-line mode
    cmd_cfg.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
    cmd_cfg.NbData = 3;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        SHELL_LOG_USER_DEBUG("QSPI mode failed, trying SPI mode...");
        
        // Fall back to SPI mode
        cmd_cfg.DataMode = HAL_OSPI_DATA_1_LINE;
        hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
        if (hal_status != HAL_OK) {
            return FLASH_STORAGE_ERROR;
        }
    }
    
    hal_status = HAL_OSPI_Receive(&hospi2, id_buffer, FLASH_TIMEOUT_MS);
    
    return (hal_status == HAL_OK) ? FLASH_STORAGE_OK : FLASH_STORAGE_ERROR;
}

/**
 * @brief Erase a 4KB sector of flash
 * @param address: Sector address to erase
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_sector_erase(uint32_t address)
{
    FlashStorageStatus_t status;
    
    // Enable write operations
    status = flash_write_enable();
    if (status != FLASH_STORAGE_OK) {
        return status;
    }
    
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = W25Q128_CMD_SECTOR_ERASE;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.Address = address;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd_cfg.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    cmd_cfg.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_NONE;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        return FLASH_STORAGE_ERROR;
    }
    
    // Wait for erase to complete
    return flash_wait_for_ready(FLASH_MAX_ERASE_TIME_MS);
}

/**
 * @brief Program a page of flash (up to 256 bytes)
 * @param address: Address to program
 * @param data: Data to write
 * @param size: Size of data (max 256 bytes)
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_page_program(uint32_t address, const uint8_t* data, uint32_t size)
{
    if (size > FLASH_PAGE_SIZE) {
        return FLASH_STORAGE_INVALID_PARAM;
    }
    
    FlashStorageStatus_t status;
    
    // Enable write operations
    status = flash_write_enable();
    if (status != FLASH_STORAGE_OK) {
        return status;
    }
    
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = W25Q128_CMD_PAGE_PROGRAM;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.Address = address;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd_cfg.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    cmd_cfg.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_1_LINE;
    cmd_cfg.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
    cmd_cfg.NbData = size;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        return FLASH_STORAGE_ERROR;
    }
    
    hal_status = HAL_OSPI_Transmit(&hospi2, (uint8_t*)data, FLASH_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        return FLASH_STORAGE_ERROR;
    }
    
    // Wait for program to complete
    return flash_wait_for_ready(FLASH_MAX_PAGE_PROGRAM_TIME_MS);
}

/**
 * @brief Read data from flash
 * @param address: Address to read from
 * @param data: Buffer to store read data
 * @param size: Size of data to read
 * @retval FlashStorageStatus_t Status
 */
static FlashStorageStatus_t flash_read_data(uint32_t address, uint8_t* data, uint32_t size)
{
    OSPI_RegularCmdTypeDef cmd_cfg = {0};
    
    cmd_cfg.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd_cfg.FlashId = HAL_OSPI_FLASH_ID_1;
    cmd_cfg.Instruction = W25Q128_CMD_READ_DATA;
    cmd_cfg.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd_cfg.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd_cfg.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd_cfg.Address = address;
    cmd_cfg.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd_cfg.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    cmd_cfg.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
    cmd_cfg.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd_cfg.DataMode = HAL_OSPI_DATA_1_LINE;
    cmd_cfg.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
    cmd_cfg.NbData = size;
    cmd_cfg.DummyCycles = 0;
    cmd_cfg.DQSMode = HAL_OSPI_DQS_DISABLE;
    cmd_cfg.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    
    HAL_StatusTypeDef hal_status = HAL_OSPI_Command(&hospi2, &cmd_cfg, FLASH_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        return FLASH_STORAGE_ERROR;
    }
    
    hal_status = HAL_OSPI_Receive(&hospi2, data, FLASH_TIMEOUT_MS);
    
    return (hal_status == HAL_OK) ? FLASH_STORAGE_OK : FLASH_STORAGE_ERROR;
}
