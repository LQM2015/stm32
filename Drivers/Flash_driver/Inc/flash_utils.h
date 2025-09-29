#ifndef FLASH_UTILS_H
#define FLASH_UTILS_H

#include "qspi_w25q256.h"
#include <string.h>

/*----------------------------------------------- Error Codes -------------------------------------------*/

#define FLASH_UTILS_OK              0       // Operation successful
#define FLASH_UTILS_ERROR           -1      // General error
#define FLASH_UTILS_TIMEOUT         -2      // Timeout error
#define FLASH_UTILS_INVALID_PARAM   -3      // Invalid parameter
#define FLASH_UTILS_NOT_ALIGNED     -4      // Address not aligned
#define FLASH_UTILS_VERIFY_FAILED   -5      // Data verification failed

/*----------------------------------------------- Structures -------------------------------------------*/

/**
 * @brief Flash statistics structure
 */
typedef struct {
    uint32_t init_time;         // Initialization timestamp
    uint32_t reads;             // Number of read operations
    uint32_t writes;            // Number of write operations
    uint32_t bytes_written;     // Total bytes written
    uint32_t sector_erases;     // Number of sector erases
    uint32_t block_erases;      // Number of block erases
    uint32_t total_write_time;  // Total write time in ms
    uint32_t total_erase_time;  // Total erase time in ms
    uint32_t read_errors;       // Read error count
    uint32_t write_errors;      // Write error count
    uint32_t erase_errors;      // Erase error count
} flash_stats_t;

/**
 * @brief Flash partition structure
 */
typedef struct {
    uint32_t start_addr;    // Start address
    uint32_t size;          // Size in bytes
    const char* name;       // Partition name
} flash_partition_t;

/*----------------------------------------------- Function Prototypes -------------------------------------------*/

/* Initialization and Status */
int8_t Flash_Utils_Init(void);                                     // Initialize Flash utilities
void Flash_Utils_PrintPartitionTable(void);                       // Print partition table
void Flash_Utils_PrintStatistics(void);                           // Print flash statistics
int8_t Flash_Utils_HealthTest(void);                              // Comprehensive health test

/* Safe Erase Operations */
int8_t Flash_Utils_EraseSector(uint32_t sector_addr);             // Safe sector erase with verification
int8_t Flash_Utils_EraseBlock(uint32_t block_addr);               // Safe block erase with verification

/* Safe Write Operations */
int8_t Flash_Utils_WriteWithErase(uint32_t address, const uint8_t* data, uint32_t size);  // Write with auto-erase

/* Data Verification */
int8_t Flash_Utils_VerifyData(uint32_t address, const uint8_t* expected_data, uint32_t size); // Verify data integrity

/* Partition Management */
const flash_partition_t* Flash_Utils_GetPartition(const char* name);  // Get partition by name

/* Utility Macros */
#define FLASH_ALIGN_SECTOR(addr)    ((addr) & 0xFFFFF000)          // Align to sector boundary
#define FLASH_ALIGN_BLOCK(addr)     ((addr) & 0xFFFF0000)          // Align to block boundary
#define FLASH_ADDR_TO_SECTOR(addr)  ((addr) / 4096)                // Convert address to sector number
#define FLASH_ADDR_TO_BLOCK(addr)   ((addr) / 65536)               // Convert address to block number

#endif // FLASH_UTILS_H