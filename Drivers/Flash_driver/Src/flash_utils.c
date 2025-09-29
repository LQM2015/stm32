/**
 ******************************************************************************
 * @file           : flash_utils.c
 * @brief          : W25Q256 Flash Utility Functions and Examples
 ******************************************************************************
 * @attention
 *
 * This file contains advanced utility functions and usage examples for
 * W25Q256 Flash operations including file-like operations, wear leveling
 * considerations, and performance optimization examples.
 *
 ******************************************************************************
 */

#include "flash_utils.h"
#include "cmsis_os2.h"

/* Flash Layout Definitions */
#define FLASH_EXT_SECTOR_SIZE           4096        // 4KB per sector
#define FLASH_BLOCK_SIZE            65536       // 64KB per block  
#define FLASH_TOTAL_SECTORS         8192        // Total sectors in 32MB
#define FLASH_TOTAL_BLOCKS          512         // Total blocks in 32MB

static const flash_partition_t flash_partitions[] = {
    {0x000000, 0x010000, "Bootloader"},      // 64KB - Bootloader
    {0x010000, 0x0F0000, "Application"},     // 960KB - Main Application
    {0x100000, 0x100000, "Config"},          // 1MB - Configuration data
    {0x200000, 0x200000, "UserData"},        // 2MB - User data
    {0x400000, 0xC00000, "FileSystem"},      // 12MB - File system
    {0x1000000, 0x1000000, "DataLog"},       // 16MB - Data logging
};

#define FLASH_PARTITION_COUNT (sizeof(flash_partitions) / sizeof(flash_partition_t))

/* Private Variables */
static uint8_t sector_buffer[FLASH_EXT_SECTOR_SIZE];    // Sector buffer
static flash_stats_t flash_statistics = {0};       // Flash statistics

/* Private Function Prototypes */
static int8_t flash_wait_ready(uint32_t timeout_ms);
static int8_t flash_verify_erase(uint32_t address, uint32_t size);

/**
 * @brief  Initialize Flash utilities
 * @retval FLASH_UTILS_OK if successful
 */
int8_t Flash_Utils_Init(void)
{
    DEBUG_FUNCTION_ENTRY();
    
    // Initialize W25Q256 driver
    int8_t status = QSPI_W25Qxx_Init();
    if(status != QSPI_W25Qxx_OK) {
        DEBUG_ERROR("Flash driver initialization failed");
        return FLASH_UTILS_ERROR;
    }
    
    // Clear statistics
    memset(&flash_statistics, 0, sizeof(flash_statistics));
    flash_statistics.init_time = HAL_GetTick();
    
    DEBUG_INFO("Flash Utils initialized successfully");
    Flash_Utils_PrintPartitionTable();
    
    DEBUG_FUNCTION_EXIT_VAL(FLASH_UTILS_OK);
    return FLASH_UTILS_OK;
}

/**
 * @brief  Safe sector erase with verification
 * @param  sector_addr: Sector address
 * @retval FLASH_UTILS_OK if successful
 */
int8_t Flash_Utils_EraseSector(uint32_t sector_addr)
{
    DEBUG_FUNCTION_ENTRY();
    
    uint32_t start_time = HAL_GetTick();
    sector_addr = QSPI_W25Qxx_GetSectorAddress(sector_addr);
    
    DEBUG_INFO("Erasing sector at 0x%08lX", sector_addr);
    
    int8_t status = QSPI_W25Qxx_SectorErase(sector_addr);
    if(status != QSPI_W25Qxx_OK) {
        DEBUG_ERROR("Sector erase failed: %s", QSPI_W25Qxx_GetErrorString(status));
        flash_statistics.erase_errors++;
        return FLASH_UTILS_ERROR;
    }
    
    // Verify erase
    status = flash_verify_erase(sector_addr, FLASH_EXT_SECTOR_SIZE);
    if(status != FLASH_UTILS_OK) {
        DEBUG_ERROR("Sector erase verification failed");
        flash_statistics.erase_errors++;
        return FLASH_UTILS_ERROR;
    }
    
    uint32_t erase_time = HAL_GetTick() - start_time;
    flash_statistics.sector_erases++;
    flash_statistics.total_erase_time += erase_time;
    
    DEBUG_INFO("Sector erase completed in %lu ms", erase_time);
    DEBUG_FUNCTION_EXIT_VAL(FLASH_UTILS_OK);
    return FLASH_UTILS_OK;
}

/**
 * @brief  Safe block erase with verification
 * @param  block_addr: Block address
 * @retval FLASH_UTILS_OK if successful
 */
int8_t Flash_Utils_EraseBlock(uint32_t block_addr)
{
    DEBUG_FUNCTION_ENTRY();
    
    uint32_t start_time = HAL_GetTick();
    block_addr = QSPI_W25Qxx_GetBlockAddress(block_addr);
    
    DEBUG_INFO("Erasing block at 0x%08lX", block_addr);
    
    int8_t status = QSPI_W25Qxx_BlockErase_64K(block_addr);
    if(status != QSPI_W25Qxx_OK) {
        DEBUG_ERROR("Block erase failed: %s", QSPI_W25Qxx_GetErrorString(status));
        flash_statistics.erase_errors++;
        return FLASH_UTILS_ERROR;
    }
    
    // Verify erase
    status = flash_verify_erase(block_addr, FLASH_BLOCK_SIZE);
    if(status != FLASH_UTILS_OK) {
        DEBUG_ERROR("Block erase verification failed");
        flash_statistics.erase_errors++;
        return FLASH_UTILS_ERROR;
    }
    
    uint32_t erase_time = HAL_GetTick() - start_time;
    flash_statistics.block_erases++;
    flash_statistics.total_erase_time += erase_time;
    
    DEBUG_INFO("Block erase completed in %lu ms", erase_time);
    DEBUG_FUNCTION_EXIT_VAL(FLASH_UTILS_OK);
    return FLASH_UTILS_OK;
}

/**
 * @brief  Safe write with automatic erase if needed
 * @param  address: Write address
 * @param  data: Data buffer
 * @param  size: Data size
 * @retval FLASH_UTILS_OK if successful
 */
int8_t Flash_Utils_WriteWithErase(uint32_t address, const uint8_t* data, uint32_t size)
{
    DEBUG_FUNCTION_ENTRY();
    
    uint32_t start_time = HAL_GetTick();
    uint32_t current_addr = address;
    uint32_t remaining = size;
    const uint8_t* write_ptr = data;
    
    DEBUG_INFO("Writing %lu bytes to 0x%08lX with auto-erase", size, address);
    
    while(remaining > 0) {
        uint32_t sector_addr = QSPI_W25Qxx_GetSectorAddress(current_addr);
        uint32_t sector_offset = current_addr - sector_addr;
        uint32_t sector_remaining = FLASH_EXT_SECTOR_SIZE - sector_offset;
        uint32_t write_size = (remaining < sector_remaining) ? remaining : sector_remaining;
        
        // Check if sector needs erasing
        if(!QSPI_W25Qxx_IsSectorEmpty(sector_addr)) {
            DEBUG_INFO("Sector at 0x%08lX is not empty, erasing...", sector_addr);
            
            // If we're not writing the full sector, we need to preserve existing data
            if(sector_offset > 0 || write_size < FLASH_EXT_SECTOR_SIZE) {
                // Read existing sector data
                if(QSPI_W25Qxx_ReadBuffer(sector_buffer, sector_addr, FLASH_EXT_SECTOR_SIZE) != QSPI_W25Qxx_OK) {
                    DEBUG_ERROR("Failed to read sector for preservation");
                    return FLASH_UTILS_ERROR;
                }
                
                // Overlay new data
                memcpy(&sector_buffer[sector_offset], write_ptr, write_size);
                
                // Erase and write full sector
                if(Flash_Utils_EraseSector(sector_addr) != FLASH_UTILS_OK) {
                    return FLASH_UTILS_ERROR;
                }
                
                if(QSPI_W25Qxx_WriteBuffer(sector_buffer, sector_addr, FLASH_EXT_SECTOR_SIZE) != QSPI_W25Qxx_OK) {
                    DEBUG_ERROR("Failed to write preserved sector data");
                    return FLASH_UTILS_ERROR;
                }
            } else {
                // Writing full sector, simple erase and write
                if(Flash_Utils_EraseSector(sector_addr) != FLASH_UTILS_OK) {
                    return FLASH_UTILS_ERROR;
                }
                
                if(QSPI_W25Qxx_WriteBuffer((uint8_t*)write_ptr, current_addr, write_size) != QSPI_W25Qxx_OK) {
                    DEBUG_ERROR("Failed to write sector data");
                    return FLASH_UTILS_ERROR;
                }
            }
        } else {
            // Sector is empty, direct write
            if(QSPI_W25Qxx_WriteBuffer((uint8_t*)write_ptr, current_addr, write_size) != QSPI_W25Qxx_OK) {
                DEBUG_ERROR("Failed to write to empty sector");
                return FLASH_UTILS_ERROR;
            }
        }
        
        current_addr += write_size;
        write_ptr += write_size;
        remaining -= write_size;
    }
    
    uint32_t write_time = HAL_GetTick() - start_time;
    flash_statistics.writes++;
    flash_statistics.bytes_written += size;
    flash_statistics.total_write_time += write_time;
    
    DEBUG_INFO("Write completed: %lu bytes in %lu ms", size, write_time);
    DEBUG_FUNCTION_EXIT_VAL(FLASH_UTILS_OK);
    return FLASH_UTILS_OK;
}

/**
 * @brief  Verify flash data integrity
 * @param  address: Address to verify
 * @param  expected_data: Expected data
 * @param  size: Data size
 * @retval FLASH_UTILS_OK if verification passes
 */
int8_t Flash_Utils_VerifyData(uint32_t address, const uint8_t* expected_data, uint32_t size)
{
    static uint8_t verify_buffer[1024];
    uint32_t remaining = size;
    uint32_t current_addr = address;
    const uint8_t* expected_ptr = expected_data;
    
    while(remaining > 0) {
        uint32_t chunk_size = (remaining > sizeof(verify_buffer)) ? sizeof(verify_buffer) : remaining;
        
        if(QSPI_W25Qxx_ReadBuffer(verify_buffer, current_addr, chunk_size) != QSPI_W25Qxx_OK) {
            DEBUG_ERROR("Failed to read data for verification");
            return FLASH_UTILS_ERROR;
        }
        
        if(memcmp(verify_buffer, expected_ptr, chunk_size) != 0) {
            // Find first mismatch
            for(uint32_t i = 0; i < chunk_size; i++) {
                if(verify_buffer[i] != expected_ptr[i]) {
                    DEBUG_ERROR("Data mismatch at 0x%08lX: expected 0x%02X, got 0x%02X", 
                               current_addr + i, expected_ptr[i], verify_buffer[i]);
                    return FLASH_UTILS_ERROR;
                }
            }
        }
        
        current_addr += chunk_size;
        expected_ptr += chunk_size;
        remaining -= chunk_size;
    }
    
    return FLASH_UTILS_OK;
}

/**
 * @brief  Get partition information by name
 * @param  name: Partition name
 * @retval Pointer to partition info or NULL
 */
const flash_partition_t* Flash_Utils_GetPartition(const char* name)
{
    for(uint32_t i = 0; i < FLASH_PARTITION_COUNT; i++) {
        if(strcmp(flash_partitions[i].name, name) == 0) {
            return &flash_partitions[i];
        }
    }
    return NULL;
}

/**
 * @brief  Print partition table
 * @retval None
 */
void Flash_Utils_PrintPartitionTable(void)
{
    DEBUG_INFO("=== Flash Partition Table ===");
    DEBUG_INFO("%-12s %-10s %-10s %-8s", "Name", "Start", "Size", "End");
    DEBUG_INFO("%-12s %-10s %-10s %-8s", "----", "-----", "----", "---");
    
    for(uint32_t i = 0; i < FLASH_PARTITION_COUNT; i++) {
        DEBUG_INFO("%-12s 0x%08lX %-8lu KB 0x%08lX", 
                   flash_partitions[i].name,
                   flash_partitions[i].start_addr,
                   flash_partitions[i].size / 1024,
                   flash_partitions[i].start_addr + flash_partitions[i].size - 1);
    }
}

/**
 * @brief  Print flash statistics
 * @retval None
 */
void Flash_Utils_PrintStatistics(void)
{
    uint32_t uptime = HAL_GetTick() - flash_statistics.init_time;
    
    DEBUG_INFO("=== Flash Statistics ===");
    DEBUG_INFO("Uptime: %lu.%03lu seconds", uptime/1000, uptime%1000);
    DEBUG_INFO("Reads: %lu", flash_statistics.reads);
    DEBUG_INFO("Writes: %lu (%lu bytes)", flash_statistics.writes, flash_statistics.bytes_written);
    DEBUG_INFO("Sector Erases: %lu", flash_statistics.sector_erases);
    DEBUG_INFO("Block Erases: %lu", flash_statistics.block_erases);
    DEBUG_INFO("Errors: %lu", flash_statistics.erase_errors + flash_statistics.write_errors + flash_statistics.read_errors);
    
    if(flash_statistics.writes > 0) {
        DEBUG_INFO("Average Write Time: %lu ms", flash_statistics.total_write_time / flash_statistics.writes);
    }
    
    if(flash_statistics.sector_erases + flash_statistics.block_erases > 0) {
        DEBUG_INFO("Average Erase Time: %lu ms", 
                   flash_statistics.total_erase_time / (flash_statistics.sector_erases + flash_statistics.block_erases));
    }
}

/**
 * @brief  Comprehensive flash health test
 * @retval FLASH_UTILS_OK if healthy
 */
int8_t Flash_Utils_HealthTest(void)
{
    DEBUG_INFO("=== Flash Health Test ===");
    
    // Test 1: ID Check
    uint32_t id = QSPI_W25Qxx_ReadID();
    if(id != W25Qxx_FLASH_ID) {
        DEBUG_ERROR("Flash ID mismatch: expected 0x%06X, got 0x%06lX", W25Qxx_FLASH_ID, id);
        return FLASH_UTILS_ERROR;
    }
    DEBUG_INFO("✓ Flash ID check passed");
    
    // Test 2: Basic Write/Read Test
    const uint32_t test_addr = 0x1FF0000; // Near end of flash
    static uint8_t test_pattern[256];
    static uint8_t read_back[256];
    
    // Generate test pattern
    for(uint16_t i = 0; i < 256; i++) {
        test_pattern[i] = (uint8_t)(i ^ 0xAA);
    }
    
    if(Flash_Utils_WriteWithErase(test_addr, test_pattern, 256) != FLASH_UTILS_OK) {
        DEBUG_ERROR("Flash write test failed");
        return FLASH_UTILS_ERROR;
    }
    
    if(QSPI_W25Qxx_ReadBuffer(read_back, test_addr, 256) != QSPI_W25Qxx_OK) {
        DEBUG_ERROR("Flash read test failed");
        return FLASH_UTILS_ERROR;
    }
    
    if(memcmp(test_pattern, read_back, 256) != 0) {
        DEBUG_ERROR("Flash data integrity test failed");
        return FLASH_UTILS_ERROR;
    }
    DEBUG_INFO("✓ Basic write/read test passed");
    
    // Test 3: Erase Test
    if(Flash_Utils_EraseSector(test_addr) != FLASH_UTILS_OK) {
        DEBUG_ERROR("Flash erase test failed");
        return FLASH_UTILS_ERROR;
    }
    DEBUG_INFO("✓ Erase test passed");
    
    DEBUG_INFO("=== Flash Health Test PASSED ===");
    return FLASH_UTILS_OK;
}

/* Private Functions */

/**
 * @brief  Wait for flash ready with timeout
 * @param  timeout_ms: Timeout in milliseconds
 * @retval FLASH_UTILS_OK if ready
 */
static int8_t flash_wait_ready(uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    
    while((HAL_GetTick() - start_time) < timeout_ms) {
        if(QSPI_W25Qxx_AutoPollingMemReady() == QSPI_W25Qxx_OK) {
            return FLASH_UTILS_OK;
        }
        osDelay(1);
    }
    
    return FLASH_UTILS_TIMEOUT;
}

/**
 * @brief  Verify that area is erased (all 0xFF)
 * @param  address: Start address
 * @param  size: Size to verify
 * @retval FLASH_UTILS_OK if erased
 */
static int8_t flash_verify_erase(uint32_t address, uint32_t size)
{
    static uint8_t verify_buffer[256];
    uint32_t remaining = size;
    uint32_t current_addr = address;
    
    while(remaining > 0) {
        uint32_t chunk_size = (remaining > sizeof(verify_buffer)) ? sizeof(verify_buffer) : remaining;
        
        if(QSPI_W25Qxx_ReadBuffer(verify_buffer, current_addr, chunk_size) != QSPI_W25Qxx_OK) {
            return FLASH_UTILS_ERROR;
        }
        
        for(uint32_t i = 0; i < chunk_size; i++) {
            if(verify_buffer[i] != 0xFF) {
                DEBUG_WARN("Erase verification failed at 0x%08lX: 0x%02X", 
                           current_addr + i, verify_buffer[i]);
                return FLASH_UTILS_ERROR;
            }
        }
        
        current_addr += chunk_size;
        remaining -= chunk_size;
    }
    
    return FLASH_UTILS_OK;
}
