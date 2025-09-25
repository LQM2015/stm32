/**
 * @file flash_storage.h
 * @brief External Flash storage management for audio data buffering
 * @date 2025-01-28
 */

#ifndef __FLASH_STORAGE_H__
#define __FLASH_STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "octospi.h"
#include <stdint.h>
#include <stdbool.h>

/* Exported constants --------------------------------------------------------*/
#define FLASH_TOTAL_SIZE               (16 * 1024 * 1024)  // 16MB total flash size
#define FLASH_STORAGE_TOTAL_SIZE       FLASH_TOTAL_SIZE     // Alias for audio_recorder compatibility

// Use different name to avoid conflict with STM32H7xx internal flash sector size
#define EXT_FLASH_SECTOR_SIZE          (4 * 1024)          // 4KB sector size for external W25Q128JV
#define FLASH_PAGE_SIZE                256                 // 256 byte page size
#define FLASH_AUDIO_START_ADDRESS      0x00000000          // Start from beginning
#define FLASH_AUDIO_END_ADDRESS        FLASH_TOTAL_SIZE    // Use full 16MB for audio

/* W25Q128 Flash Commands */
#define FLASH_CMD_WRITE_ENABLE         0x06
#define FLASH_CMD_WRITE_DISABLE        0x04
#define FLASH_CMD_READ_STATUS_REG1     0x05
#define FLASH_CMD_READ_STATUS_REG2     0x35
#define FLASH_CMD_WRITE_STATUS_REG     0x01
#define FLASH_CMD_PAGE_PROGRAM         0x02
#define FLASH_CMD_QUAD_PAGE_PROGRAM    0x32
#define FLASH_CMD_SECTOR_ERASE         0x20
#define FLASH_CMD_BLOCK_ERASE_32K      0x52
#define FLASH_CMD_BLOCK_ERASE_64K      0xD8
#define FLASH_CMD_CHIP_ERASE           0xC7
#define FLASH_CMD_READ_DATA            0x03
#define FLASH_CMD_FAST_READ            0x0B
#define FLASH_CMD_QUAD_READ            0xEB
#define FLASH_CMD_READ_JEDEC_ID        0x9F
#define FLASH_CMD_READ_UNIQUE_ID       0x4B

/* Status Register Bits */
#define FLASH_STATUS_BUSY              0x01
#define FLASH_STATUS_WEL               0x02
#define FLASH_STATUS_BP0               0x04
#define FLASH_STATUS_BP1               0x08
#define FLASH_STATUS_BP2               0x10
#define FLASH_STATUS_TB                0x20
#define FLASH_STATUS_SEC               0x40
#define FLASH_STATUS_SRP0              0x80

/* Exported types ------------------------------------------------------------*/
typedef enum {
    FLASH_STORAGE_OK = 0,
    FLASH_STORAGE_ERROR,
    FLASH_STORAGE_BUSY,
    FLASH_STORAGE_TIMEOUT,
    FLASH_STORAGE_FULL,
    FLASH_STORAGE_INVALID_PARAM
} FlashStorageStatus_t;

typedef struct {
    uint32_t write_address;        // Current write position in flash
    uint32_t read_address;         // Current read position in flash  
    uint32_t total_bytes_written;  // Total bytes written to flash
    uint32_t available_space;      // Remaining space in flash
    bool is_initialized;           // Flash initialization status
    bool is_full;                  // Flash full status
} FlashStorageManager_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Initialize external flash storage
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_init(void);

/**
 * @brief Deinitialize external flash storage
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_deinit(void);

/**
 * @brief Erase all audio data in flash (prepare for new recording)
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_erase_audio_area(void);

/**
 * @brief Write audio data to flash
 * @param data: Pointer to audio data
 * @param size: Size of data in bytes
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_write_audio_data(const uint8_t* data, uint32_t size);

/**
 * @brief Read audio data from flash
 * @param data: Pointer to buffer for read data
 * @param size: Size of data to read
 * @param address: Address in flash to read from (relative to audio start)
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_read_audio_data(uint8_t* data, uint32_t size, uint32_t address);

/**
 * @brief Copy all audio data from flash to TF card file
 * @param filename: Target filename on TF card
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_copy_to_tf_card(const char* filename);

/**
 * @brief Check if flash storage is full
 * @retval bool True if full, false otherwise
 */
bool flash_storage_is_full(void);

/**
 * @brief Get current storage information
 * @param manager: Pointer to storage manager structure to fill
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_get_info(FlashStorageManager_t* manager);

/**
 * @brief Get remaining available space in bytes
 * @retval uint32_t Available space in bytes
 */
uint32_t flash_storage_get_available_space(void);

/**
 * @brief Get total bytes written to flash
 * @retval uint32_t Total bytes written
 */
uint32_t flash_storage_get_total_written(void);

/**
 * @brief Test flash storage functionality
 * @retval FlashStorageStatus_t Status
 */
FlashStorageStatus_t flash_storage_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_STORAGE_H__ */