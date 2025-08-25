/**
 * @file shell_commands.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell commands for STM32H725 project
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#include "shell.h"
#include "shell_port.h"
#include "shell_log.h"
#include "main.h"
#include "clock_management.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "ff.h"
#include "diskio.h"
#include "usbd_core.h"
#include "usb_device.h"
#include "usbd_storage_if.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 外部变量声明
extern SD_HandleTypeDef hsd1;
extern USBD_HandleTypeDef hUsbDeviceHS;

// 外部函数声明
extern int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);

/* 系统信息命令 */
int cmd_sysinfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("System information requested");
    
    SHELL_LOG_SYS_INFO("=== STM32H725 System Information ===");
    SHELL_LOG_SYS_INFO("CPU ID: 0x%08lX", HAL_GetDEVID());
    SHELL_LOG_SYS_INFO("CPU Rev: 0x%08lX", HAL_GetREVID());
    SHELL_LOG_SYS_INFO("UID: %08lX-%08lX-%08lX", 
           HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    SHELL_LOG_SYS_INFO("System Clock: %lu Hz", HAL_RCC_GetSysClockFreq());
    SHELL_LOG_SYS_INFO("HCLK: %lu Hz", HAL_RCC_GetHCLKFreq());
    SHELL_LOG_SYS_INFO("PCLK1: %lu Hz", HAL_RCC_GetPCLK1Freq());
    SHELL_LOG_SYS_INFO("PCLK2: %lu Hz", HAL_RCC_GetPCLK2Freq());
    SHELL_LOG_SYS_INFO("Tick: %lu ms", HAL_GetTick());
    SHELL_LOG_SYS_INFO("HAL Version: %lu", HAL_GetHalVersion());
    
    // 记录系统状态到日志
    SHELL_LOG_SYS_DEBUG("CPU ID: 0x%08lX, Rev: 0x%08lX", HAL_GetDEVID(), HAL_GetREVID());
    SHELL_LOG_SYS_DEBUG("System Clock: %lu Hz, HCLK: %lu Hz", 
                        HAL_RCC_GetSysClockFreq(), HAL_RCC_GetHCLKFreq());
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sysinfo, cmd_sysinfo, show system information);

/* SD卡诊断命令 */
int cmd_sddiag(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== SD Card Diagnostic ===");
    
    // 检查SD卡状态
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd1);
    SHELL_LOG_SYS_INFO("SD Card State: %d", cardState);
    
    if (cardState != HAL_SD_CARD_TRANSFER) {
        SHELL_LOG_SYS_ERROR("SD Card not in transfer state!");
        return -1;
    }
    
    // 获取SD卡信息
    HAL_SD_CardInfoTypeDef cardInfo;
    HAL_StatusTypeDef status = HAL_SD_GetCardInfo(&hsd1, &cardInfo);
    if (status == HAL_OK) {
        SHELL_LOG_SYS_INFO("Card Type: %lu", cardInfo.CardType);
        SHELL_LOG_SYS_INFO("Card Version: %lu", cardInfo.CardVersion);
        SHELL_LOG_SYS_INFO("Block Number: %lu", cardInfo.BlockNbr);
        SHELL_LOG_SYS_INFO("Block Size: %lu", cardInfo.BlockSize);
        SHELL_LOG_SYS_INFO("Logical Block Number: %lu", cardInfo.LogBlockNbr);
        SHELL_LOG_SYS_INFO("Logical Block Size: %lu", cardInfo.LogBlockSize);
        
        uint64_t totalSize = (uint64_t)cardInfo.LogBlockNbr * cardInfo.LogBlockSize;
        SHELL_LOG_SYS_INFO("Total Capacity: %llu bytes", totalSize);
    } else {
        SHELL_LOG_SYS_ERROR("Failed to get card info: %d", status);
        return -1;
    }
    
    // 读取并显示MBR（第0扇区）
    static uint8_t sector_buffer[512];
    status = HAL_SD_ReadBlocks(&hsd1, sector_buffer, 0, 1, 5000);
    if (status == HAL_OK) {
        SHELL_LOG_SYS_INFO("=== MBR Content (first 32 bytes) ===");
        for (int i = 0; i < 32; i += 16) {
            SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                               i,
                               sector_buffer[i+0], sector_buffer[i+1], sector_buffer[i+2], sector_buffer[i+3],
                               sector_buffer[i+4], sector_buffer[i+5], sector_buffer[i+6], sector_buffer[i+7],
                               sector_buffer[i+8], sector_buffer[i+9], sector_buffer[i+10], sector_buffer[i+11],
                               sector_buffer[i+12], sector_buffer[i+13], sector_buffer[i+14], sector_buffer[i+15]);
        }
        
        // 检查分区表签名
        if (sector_buffer[510] == 0x55 && sector_buffer[511] == 0xAA) {
            SHELL_LOG_SYS_INFO("Valid MBR signature found (0x55AA)");
        } else {
            SHELL_LOG_SYS_WARNING("No valid MBR signature found! Expected 0x55AA, got 0x%02X%02X", 
                                  sector_buffer[511], sector_buffer[510]);
        }
        
        // 检查FAT文件系统签名
        if (strncmp((char*)&sector_buffer[54], "FAT", 3) == 0) {
            SHELL_LOG_SYS_INFO("FAT filesystem detected in boot sector");
        } else if (strncmp((char*)&sector_buffer[82], "FAT32", 5) == 0) {
            SHELL_LOG_SYS_INFO("FAT32 filesystem detected in boot sector");
        } else {
            SHELL_LOG_SYS_WARNING("No FAT filesystem signature found in boot sector");
        }
        
    } else {
        SHELL_LOG_SYS_ERROR("Failed to read MBR: %d", status);
        return -1;
    }
    
    SHELL_LOG_SYS_INFO("SD Card diagnostic completed");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sddiag, cmd_sddiag, diagnose SD card and filesystem);

/* USB重新初始化命令 */
int cmd_usb_reinit(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== USB Re-initialization ===");
    
    // 断开USB连接
    SHELL_LOG_SYS_INFO("Disconnecting USB...");
    USBD_DeInit(&hUsbDeviceHS);
    
    // 等待一段时间
    osDelay(1000);
    
    // 重新初始化USB
    SHELL_LOG_SYS_INFO("Re-initializing USB...");
    extern void MX_USB_DEVICE_Init(void);
    MX_USB_DEVICE_Init();
    
    SHELL_LOG_SYS_INFO("USB re-initialization completed");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 usb_reinit, cmd_usb_reinit, reinitialize USB device);

/* SD卡格式化命令 - 简化版本 */
int cmd_format_sd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== SD Card Format Instructions ===");
    SHELL_LOG_SYS_WARNING("SD card appears to be uninitialized (MBR is empty)");
    SHELL_LOG_SYS_INFO("Please follow these steps to format the SD card:");
    SHELL_LOG_SYS_INFO("1. Remove SD card from STM32 device");
    SHELL_LOG_SYS_INFO("2. Insert SD card into PC card reader");
    SHELL_LOG_SYS_INFO("3. Open Windows Disk Management or use format command:");
    SHELL_LOG_SYS_INFO("   - Format as FAT32");
    SHELL_LOG_SYS_INFO("   - Allocation unit size: Default");
    SHELL_LOG_SYS_INFO("   - Quick format: Unchecked (full format recommended)");
    SHELL_LOG_SYS_INFO("4. Safely eject SD card from PC");
    SHELL_LOG_SYS_INFO("5. Insert SD card back into STM32 device");
    SHELL_LOG_SYS_INFO("6. Reset STM32 device");
    SHELL_LOG_SYS_INFO("7. Run 'sddiag' command to verify");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 format_sd, cmd_format_sd, format SD card with FAT filesystem);

/* USB状态检查命令 */
int cmd_usb_status(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== USB Status Check ===");
    SHELL_LOG_SYS_INFO("USB Device State: %d", hUsbDeviceHS.dev_state);
    SHELL_LOG_SYS_INFO("USB Device Speed: %d", hUsbDeviceHS.dev_config);
    SHELL_LOG_SYS_INFO("USB Device Address: %d", hUsbDeviceHS.dev_address);
    
    // 检查SD卡状态以确保存储后端正常
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd1);
    SHELL_LOG_SYS_INFO("SD Card Backend State: %d", cardState);
    
    if (cardState == HAL_SD_CARD_TRANSFER) {
        SHELL_LOG_SYS_INFO("Storage backend is ready");
        
        // 检查SD卡是否有有效的文件系统
        static uint8_t sector_buffer[512];
        HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&hsd1, sector_buffer, 0, 1, 5000);
        if (status == HAL_OK) {
            if (sector_buffer[510] == 0x55 && sector_buffer[511] == 0xAA) {
                // 检查是否有分区表
                uint8_t hasValidPartition = 0;
                for (int i = 0; i < 4; i++) {
                    uint8_t *partition = &sector_buffer[446 + i * 16];
                    if (partition[4] != 0x00) { // 分区类型不为0
                        hasValidPartition = 1;
                        SHELL_LOG_SYS_INFO("Found partition %d: Type=0x%02X", i, partition[4]);
                        break;
                    }
                }
                
                if (!hasValidPartition) {
                    SHELL_LOG_SYS_WARNING("No valid partitions found - SD card needs formatting");
                } else {
                    SHELL_LOG_SYS_INFO("SD card appears to have valid partition(s)");
                }
            } else {
                SHELL_LOG_SYS_WARNING("Invalid MBR signature - SD card needs formatting");
            }
        } else {
            SHELL_LOG_SYS_ERROR("Failed to read SD card MBR");
        }
    } else {
        SHELL_LOG_SYS_ERROR("Storage backend not ready!");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 usb_status, cmd_usb_status, check USB device and storage status);

/* 详细的分区表诊断命令 */
int cmd_partition_info(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== Detailed Partition Table Analysis ===");
    
    // 读取MBR
    static uint8_t sector_buffer[512];
    HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&hsd1, sector_buffer, 0, 1, 5000);
    if (status != HAL_OK) {
        SHELL_LOG_SYS_ERROR("Failed to read MBR: %d", status);
        return -1;
    }
    
    // 检查MBR签名
    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
        SHELL_LOG_SYS_ERROR("Invalid MBR signature: 0x%02X%02X", sector_buffer[511], sector_buffer[510]);
        return -1;
    }
    
    SHELL_LOG_SYS_INFO("Valid MBR signature found");
    
    // 分析每个分区表条目
    for (int i = 0; i < 4; i++) {
        uint8_t *partition = &sector_buffer[446 + i * 16];
        
        uint8_t status_byte = partition[0];
        uint8_t partition_type = partition[4];
        uint32_t start_lba = *(uint32_t*)&partition[8];
        uint32_t size_sectors = *(uint32_t*)&partition[12];
        
        SHELL_LOG_SYS_INFO("=== Partition %d ===", i);
        SHELL_LOG_SYS_INFO("Status: 0x%02X %s", status_byte, 
                           (status_byte == 0x80) ? "(Bootable)" : 
                           (status_byte == 0x00) ? "(Non-bootable)" : "(Invalid)");
        SHELL_LOG_SYS_INFO("Type: 0x%02X %s", partition_type,
                           (partition_type == 0x0C) ? "(FAT32 LBA)" :
                           (partition_type == 0x0B) ? "(FAT32)" :
                           (partition_type == 0x06) ? "(FAT16)" :
                           (partition_type == 0x00) ? "(Empty)" : "(Other)");
        SHELL_LOG_SYS_INFO("Start LBA: %lu", start_lba);
        SHELL_LOG_SYS_INFO("Size: %lu sectors (%lu MB)", size_sectors, (size_sectors * 512) / (1024*1024));
        
        if (partition_type != 0x00) {
            // 读取分区的第一个扇区（可能是FAT引导扇区）
            if (start_lba > 0 && start_lba < 30572544) {
                static uint8_t boot_sector[512];
                status = HAL_SD_ReadBlocks(&hsd1, boot_sector, start_lba, 1, 5000);
                if (status == HAL_OK) {
                    // 检查FAT引导扇区签名
                    if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
                        SHELL_LOG_SYS_INFO("Valid boot sector signature in partition %d", i);
                        
                        // 检查文件系统类型
                        if (strncmp((char*)&boot_sector[54], "FAT", 3) == 0) {
                            SHELL_LOG_SYS_INFO("FAT12/16 filesystem detected");
                        } else if (strncmp((char*)&boot_sector[82], "FAT32", 5) == 0) {
                            SHELL_LOG_SYS_INFO("FAT32 filesystem detected");
                        } else {
                            SHELL_LOG_SYS_WARNING("Unknown filesystem in partition %d", i);
                        }
                        
                        // 显示一些FAT参数
                        uint16_t bytes_per_sector = *(uint16_t*)&boot_sector[11];
                        uint8_t sectors_per_cluster = boot_sector[13];
                        uint16_t reserved_sectors = *(uint16_t*)&boot_sector[14];
                        uint8_t num_fats = boot_sector[16];
                        
                        SHELL_LOG_SYS_INFO("Bytes per sector: %u", bytes_per_sector);
                        SHELL_LOG_SYS_INFO("Sectors per cluster: %u", sectors_per_cluster);
                        SHELL_LOG_SYS_INFO("Reserved sectors: %u", reserved_sectors);
                        SHELL_LOG_SYS_INFO("Number of FATs: %u", num_fats);
                        
                    } else {
                        SHELL_LOG_SYS_WARNING("Invalid boot sector signature in partition %d", i);
                    }
                } else {
                    SHELL_LOG_SYS_ERROR("Failed to read boot sector for partition %d", i);
                }
            }
        }
        SHELL_LOG_SYS_INFO("");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 partition_info, cmd_partition_info, analyze partition table in detail);

/* USB诊断和故障排除命令 */
int cmd_usb_troubleshoot(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== USB Mass Storage Troubleshooting ===");
    
    // 1. 检查USB设备状态
    SHELL_LOG_SYS_INFO("1. USB Device Status:");
    SHELL_LOG_SYS_INFO("   State: %d %s", hUsbDeviceHS.dev_state,
                       (hUsbDeviceHS.dev_state == 3) ? "(Configured)" :
                       (hUsbDeviceHS.dev_state == 2) ? "(Addressed)" :
                       (hUsbDeviceHS.dev_state == 1) ? "(Default)" : "(Unknown)");
    SHELL_LOG_SYS_INFO("   Address: %d", hUsbDeviceHS.dev_address);
    SHELL_LOG_SYS_INFO("   Config: %d", hUsbDeviceHS.dev_config);
    
    // 2. 检查SD卡状态
    SHELL_LOG_SYS_INFO("2. SD Card Status:");
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd1);
    SHELL_LOG_SYS_INFO("   Card State: %d %s", cardState,
                       (cardState == 4) ? "(Transfer Ready)" : "(Not Ready)");
    
    // 3. 检查存储容量报告
    HAL_SD_CardInfoTypeDef cardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &cardInfo) == HAL_OK) {
        SHELL_LOG_SYS_INFO("3. Storage Capacity:");
        SHELL_LOG_SYS_INFO("   Logical Blocks: %lu", cardInfo.LogBlockNbr);
        SHELL_LOG_SYS_INFO("   Block Size: %lu", cardInfo.LogBlockSize);
        uint64_t totalMB = ((uint64_t)cardInfo.LogBlockNbr * cardInfo.LogBlockSize) / (1024*1024);
        SHELL_LOG_SYS_INFO("   Total Size: %llu MB", totalMB);
    }
    
    // 4. 检查分区表
    SHELL_LOG_SYS_INFO("4. Partition Check:");
    static uint8_t sector_buffer[512];
    if (HAL_SD_ReadBlocks(&hsd1, sector_buffer, 0, 1, 5000) == HAL_OK) {
        if (sector_buffer[510] == 0x55 && sector_buffer[511] == 0xAA) {
            SHELL_LOG_SYS_INFO("   MBR Signature: Valid");
            
            // 检查第一个分区
            uint8_t *partition = &sector_buffer[446];
            if (partition[4] != 0x00) {
                SHELL_LOG_SYS_INFO("   Primary Partition: Type 0x%02X, Start %lu, Size %lu",
                                   partition[4], *(uint32_t*)&partition[8], *(uint32_t*)&partition[12]);
            } else {
                SHELL_LOG_SYS_WARNING("   No primary partition found!");
            }
        } else {
            SHELL_LOG_SYS_ERROR("   Invalid MBR signature!");
        }
    }
    
    // 5. 建议的解决方案
    SHELL_LOG_SYS_INFO("5. Troubleshooting Recommendations:");
    
    if (hUsbDeviceHS.dev_state != 3) {
        SHELL_LOG_SYS_WARNING("   - USB not properly enumerated, try usb_reinit");
    }
    
    if (cardState != 4) {
        SHELL_LOG_SYS_WARNING("   - SD card not ready, check hardware connection");
    } else {
        SHELL_LOG_SYS_INFO("   - Hardware appears functional");
        SHELL_LOG_SYS_INFO("   - If PC still doesn't recognize:");
        SHELL_LOG_SYS_INFO("     a) Try different USB cable");
        SHELL_LOG_SYS_INFO("     b) Try different PC/USB port"); 
        SHELL_LOG_SYS_INFO("     c) Check Windows Device Manager");
        SHELL_LOG_SYS_INFO("     d) Format SD card on PC if needed");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 usb_troubleshoot, cmd_usb_troubleshoot, comprehensive USB troubleshooting);

/* 测试读取特定扇区的命令 */
int cmd_read_sector(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    uint32_t sector_addr = 0;
    if (argc > 1) {
        sector_addr = strtoul(argv[1], NULL, 0);
    }
    
    SHELL_LOG_SYS_INFO("=== Read Sector %lu ===", sector_addr);
    
    static uint8_t sector_buffer[512];
    HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&hsd1, sector_buffer, sector_addr, 1, 5000);
    
    if (status != HAL_OK) {
        SHELL_LOG_SYS_ERROR("Failed to read sector %lu: %d", sector_addr, status);
        return -1;
    }
    
    // 显示扇区数据的前64字节
    SHELL_LOG_SYS_INFO("Sector %lu content (first 64 bytes):", sector_addr);
    for (int i = 0; i < 64; i += 16) {
        SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                           i,
                           sector_buffer[i+0], sector_buffer[i+1], sector_buffer[i+2], sector_buffer[i+3],
                           sector_buffer[i+4], sector_buffer[i+5], sector_buffer[i+6], sector_buffer[i+7],
                           sector_buffer[i+8], sector_buffer[i+9], sector_buffer[i+10], sector_buffer[i+11],
                           sector_buffer[i+12], sector_buffer[i+13], sector_buffer[i+14], sector_buffer[i+15]);
    }
    
    // 显示扇区末尾的签名
    SHELL_LOG_SYS_INFO("Last 16 bytes (including signature):");
    SHELL_LOG_SYS_INFO("01F0: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                       sector_buffer[496], sector_buffer[497], sector_buffer[498], sector_buffer[499],
                       sector_buffer[500], sector_buffer[501], sector_buffer[502], sector_buffer[503],
                       sector_buffer[504], sector_buffer[505], sector_buffer[506], sector_buffer[507],
                       sector_buffer[508], sector_buffer[509], sector_buffer[510], sector_buffer[511]);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 read_sector, cmd_read_sector, read and display specific sector content);

/* 测试USB存储读取函数 */
int cmd_test_usb_read(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    uint32_t sector_addr = 0;
    if (argc > 1) {
        sector_addr = strtoul(argv[1], NULL, 0);
    }
    
    SHELL_LOG_SYS_INFO("=== Test USB Storage Read Function ===");
    SHELL_LOG_SYS_INFO("Testing sector %lu", sector_addr);
    
    // 分配一个与USB使用相同的缓冲区
    static uint8_t usb_buffer[512] __attribute__((aligned(32)));
    
    SHELL_LOG_SYS_INFO("USB buffer address: 0x%08lX", (uint32_t)usb_buffer);
    SHELL_LOG_SYS_INFO("Buffer alignment (& 0x1F): 0x%02X", (uint32_t)usb_buffer & 0x1F);
    
    // 清零缓冲区
    memset(usb_buffer, 0xFF, sizeof(usb_buffer));
    
    // 调用USB存储读取函数
    int8_t result = STORAGE_Read_HS(0, usb_buffer, sector_addr, 1);
    
    if (result == USBD_OK) {
        SHELL_LOG_SYS_INFO("USB Storage Read successful");
        
        // 显示读取的数据
        SHELL_LOG_SYS_INFO("USB buffer content (first 64 bytes):");
        for (int i = 0; i < 64; i += 16) {
            SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                               i,
                               usb_buffer[i+0], usb_buffer[i+1], usb_buffer[i+2], usb_buffer[i+3],
                               usb_buffer[i+4], usb_buffer[i+5], usb_buffer[i+6], usb_buffer[i+7],
                               usb_buffer[i+8], usb_buffer[i+9], usb_buffer[i+10], usb_buffer[i+11],
                               usb_buffer[i+12], usb_buffer[i+13], usb_buffer[i+14], usb_buffer[i+15]);
        }
        
        // 显示签名区域
        SHELL_LOG_SYS_INFO("Signature bytes [510:511]: 0x%02X%02X", usb_buffer[511], usb_buffer[510]);
        
    } else {
        SHELL_LOG_SYS_ERROR("USB Storage Read failed: %d", result);
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_usb_read, cmd_test_usb_read, test USB storage read function directly);

/* 测试缓存对齐效果的命令 */
int cmd_cache_test(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== Cache Alignment Test ===");
    
    // 测试不对齐的缓冲区
    static uint8_t unaligned_buffer[513];  // 故意不对齐
    uint8_t *test_buf = unaligned_buffer + 1;  // 偏移1字节使其不对齐
    
    SHELL_LOG_SYS_INFO("Unaligned buffer address: 0x%08lX", (uint32_t)test_buf);
    SHELL_LOG_SYS_INFO("Alignment (& 0x1F): 0x%02X", (uint32_t)test_buf & 0x1F);
    
    uint32_t aligned_addr = (uint32_t)test_buf & ~0x1F;
    uint32_t aligned_size = (512 + 31) & ~0x1F;
    SHELL_LOG_SYS_INFO("Corrected aligned address: 0x%08lX", aligned_addr);
    SHELL_LOG_SYS_INFO("Corrected aligned size: %lu", aligned_size);
    
    // 测试直接读取
    DRESULT res = disk_read(0, test_buf, 0, 1);
    if (res == RES_OK) {
        SHELL_LOG_SYS_INFO("Direct disk_read successful");
        SHELL_LOG_SYS_INFO("First 32 bytes:");
        for (int i = 0; i < 32; i += 16) {
            SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                i, test_buf[i], test_buf[i+1], test_buf[i+2], test_buf[i+3],
                test_buf[i+4], test_buf[i+5], test_buf[i+6], test_buf[i+7],
                test_buf[i+8], test_buf[i+9], test_buf[i+10], test_buf[i+11],
                test_buf[i+12], test_buf[i+13], test_buf[i+14], test_buf[i+15]);
        }
    } else {
        SHELL_LOG_SYS_ERROR("Direct disk_read failed: %d", res);
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 cache_test, cmd_cache_test, test cache alignment effects);

/* 对比USB和SD卡读取的命令 */
int cmd_compare_read(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("=== Compare USB vs Direct SD Card Read ===");
    
    static uint8_t usb_buffer[512] __attribute__((aligned(32)));
    static uint8_t sd_buffer[512] __attribute__((aligned(32)));
    
    SHELL_LOG_SYS_INFO("USB buffer: 0x%08lX, SD buffer: 0x%08lX", 
                      (uint32_t)usb_buffer, (uint32_t)sd_buffer);
    
    // 清空缓冲区
    memset(usb_buffer, 0xAA, sizeof(usb_buffer));
    memset(sd_buffer, 0x55, sizeof(sd_buffer));
    
    // 1. 直接SD卡读取
    SHELL_LOG_SYS_INFO("1. Direct SD card read:");
    DRESULT sd_res = disk_read(0, sd_buffer, 0, 1);
    if (sd_res == RES_OK) {
        SHELL_LOG_SYS_INFO("SD read successful, first 32 bytes:");
        for (int i = 0; i < 32; i += 16) {
            SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                i, sd_buffer[i], sd_buffer[i+1], sd_buffer[i+2], sd_buffer[i+3],
                sd_buffer[i+4], sd_buffer[i+5], sd_buffer[i+6], sd_buffer[i+7],
                sd_buffer[i+8], sd_buffer[i+9], sd_buffer[i+10], sd_buffer[i+11],
                sd_buffer[i+12], sd_buffer[i+13], sd_buffer[i+14], sd_buffer[i+15]);
        }
    } else {
        SHELL_LOG_SYS_ERROR("SD read failed: %d", sd_res);
    }
    
    // 2. USB存储接口读取
    SHELL_LOG_SYS_INFO("2. USB storage interface read:");
    int8_t usb_res = STORAGE_Read_HS(0, usb_buffer, 0, 1);
    if (usb_res == USBD_OK) {
        SHELL_LOG_SYS_INFO("USB read successful, first 32 bytes:");
        for (int i = 0; i < 32; i += 16) {
            SHELL_LOG_SYS_INFO("%04X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                i, usb_buffer[i], usb_buffer[i+1], usb_buffer[i+2], usb_buffer[i+3],
                usb_buffer[i+4], usb_buffer[i+5], usb_buffer[i+6], usb_buffer[i+7],
                usb_buffer[i+8], usb_buffer[i+9], usb_buffer[i+10], usb_buffer[i+11],
                usb_buffer[i+12], usb_buffer[i+13], usb_buffer[i+14], usb_buffer[i+15]);
        }
    } else {
        SHELL_LOG_SYS_ERROR("USB read failed: %d", usb_res);
    }
    
    // 3. 对比数据
    SHELL_LOG_SYS_INFO("3. Data comparison:");
    int differences = 0;
    for (int i = 0; i < 512; i++) {
        if (sd_buffer[i] != usb_buffer[i]) {
            differences++;
            if (differences <= 10) {  // 只显示前10个不同之处
                SHELL_LOG_SYS_INFO("Diff at [%d]: SD=0x%02X, USB=0x%02X", 
                                  i, sd_buffer[i], usb_buffer[i]);
            }
        }
    }
    SHELL_LOG_SYS_INFO("Total differences: %d/512 bytes", differences);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 compare_read, cmd_compare_read, compare USB vs direct SD card read);

/* 内存信息命令 */
int cmd_meminfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
    
    SHELL_LOG_MEM_INFO("Memory status requested");
    
    SHELL_LOG_MEM_INFO("=== Memory Information ===");
    SHELL_LOG_MEM_INFO("Free Heap: %u bytes", free_heap);
    SHELL_LOG_MEM_INFO("Min Free Heap: %u bytes", min_free_heap);
    SHELL_LOG_MEM_INFO("Used Heap: %u bytes", configTOTAL_HEAP_SIZE - free_heap);
    SHELL_LOG_MEM_INFO("Total Heap: %u bytes", configTOTAL_HEAP_SIZE);
    
    float usage_percent = ((float)(configTOTAL_HEAP_SIZE - free_heap) / configTOTAL_HEAP_SIZE) * 100;
    SHELL_LOG_MEM_INFO("Memory Usage: %.1f%%", usage_percent);
    
    // 添加内存状态日志
    if (usage_percent > 80.0f) {
        SHELL_LOG_MEM_WARNING("High memory usage: %.1f%% (%u/%u bytes)", 
                             usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    } else if (usage_percent > 60.0f) {
        SHELL_LOG_MEM_INFO("Memory usage: %.1f%% (%u/%u bytes)", 
                          usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    } else {
        SHELL_LOG_MEM_DEBUG("Memory usage: %.1f%% (%u/%u bytes)", 
                           usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 meminfo, cmd_meminfo, show memory information);

/* 任务信息命令 */
int cmd_taskinfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_TASK_INFO("Task information requested");
    
    SHELL_LOG_TASK_INFO("=== FreeRTOS Task Information ===");
    SHELL_LOG_TASK_INFO("Kernel State: %s", 
           (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? "Running" : "Not Running");
    SHELL_LOG_TASK_INFO("Kernel Tick: %lu", xTaskGetTickCount());
    SHELL_LOG_TASK_INFO("Kernel Frequency: %lu Hz", (uint32_t)configTICK_RATE_HZ);
    
    // 记录任务调度器状态
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        SHELL_LOG_TASK_DEBUG("FreeRTOS scheduler is running, tick: %lu", xTaskGetTickCount());
    } else {
        SHELL_LOG_TASK_WARNING("FreeRTOS scheduler is not running");
    }
    
    #if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    char *pcWriteBuffer = pvPortMalloc(1024);
    if (pcWriteBuffer != NULL) {
        vTaskList(pcWriteBuffer);
        SHELL_LOG_TASK_INFO("Task List:\n%s", pcWriteBuffer);
        SHELL_LOG_TASK_DEBUG("Task list generated successfully");
        vPortFree(pcWriteBuffer);
    } else {
        SHELL_LOG_TASK_ERROR("Failed to allocate memory for task list");
    }
    #endif
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 taskinfo, cmd_taskinfo, show task information);

/* 重启命令 */
int cmd_reboot(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("System reboot requested by user");
    SHELL_LOG_SYS_WARNING("System rebooting...");
    
    SHELL_LOG_SYS_INFO("System rebooting in 100ms...");
    HAL_Delay(100);
    NVIC_SystemReset();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 reboot, cmd_reboot, reboot system);


/* LED控制命令（如果有LED的话） */
int cmd_led(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_WARNING("LED command called without parameters");
        SHELL_LOG_USER_INFO("Usage: led <on|off|toggle>");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        SHELL_LOG_USER_INFO("LED turned ON");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else if (strcmp(argv[1], "off") == 0) {
        SHELL_LOG_USER_INFO("LED turned OFF");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    } else if (strcmp(argv[1], "toggle") == 0) {
        SHELL_LOG_USER_INFO("LED toggled");
        // HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    } else {
        SHELL_LOG_USER_ERROR("Invalid LED parameter: %s", argv[1]);
        SHELL_LOG_USER_ERROR("Invalid parameter. Use: on, off, or toggle");
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 led, cmd_led, control LED);

/* 时钟配置测试命令 */
int cmd_clocktest(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_CLK_INFO("Starting clock profile test");
    
    TestAllClockProfiles();
    
    SHELL_LOG_CLK_INFO("Clock profile test completed");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 clocktest, cmd_clocktest, test all clock profiles);

/* 时钟切换命令 */
int cmd_setclock(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: setclock <profile>");
        SHELL_LOG_USER_INFO("Available profiles:");
        SHELL_LOG_USER_INFO("  0 - 32kHz (Ultra Low Power - LSI)");
        SHELL_LOG_USER_INFO("  1 - 24MHz (Low Power - HSI/4)");
        SHELL_LOG_USER_INFO("  2 - 48MHz (Energy Saving)");
        SHELL_LOG_USER_INFO("  3 - 96MHz (Balanced)");
        SHELL_LOG_USER_INFO("  4 - 128MHz (Standard)");
        SHELL_LOG_USER_INFO("  5 - 200MHz (High Efficiency)");
        SHELL_LOG_USER_INFO("  6 - 300MHz (High Performance)");
        SHELL_LOG_USER_INFO("  7 - 400MHz (Ultra High Performance)");
        SHELL_LOG_USER_INFO("  8 - 550MHz (Maximum Performance)");
        SHELL_LOG_USER_INFO("Current System Clock: %lu Hz (%.1f MHz)", 
               HAL_RCC_GetSysClockFreq(), HAL_RCC_GetSysClockFreq() / 1000000.0f);
        return 0;
    }
    
    int profile = atoi(argv[1]);
    if (profile < 0 || profile > 8) {
        SHELL_LOG_CLK_ERROR("Invalid clock profile %d requested", profile);
        SHELL_LOG_CLK_ERROR("Error: Invalid profile %d. Valid range: 0-8", profile);
        return -1;
    }
    
    SHELL_LOG_CLK_INFO("Switching to clock profile %d", profile);
    
    // 记录切换前的时钟频率
    uint32_t old_freq = HAL_RCC_GetSysClockFreq();
    SHELL_LOG_CLK_DEBUG("Current frequency before switch: %lu Hz", old_freq);
    
    // 执行时钟切换
    ClockProfile_t clock_profile = (ClockProfile_t)profile;
    if (SwitchSystemClock(clock_profile) == HAL_OK) {
        // 切换成功，显示新的时钟频率
        uint32_t new_freq = HAL_RCC_GetSysClockFreq();
        SHELL_LOG_CLK_INFO("Clock switch successful: %lu Hz -> %lu Hz", old_freq, new_freq);
        
        SHELL_LOG_CLK_INFO("Clock switch successful!");
        SHELL_LOG_CLK_INFO("Previous: %lu Hz (%.1f MHz)", old_freq, old_freq / 1000000.0f);
        SHELL_LOG_CLK_INFO("Current:  %lu Hz (%.1f MHz)", new_freq, new_freq / 1000000.0f);
        
        // 显示其他时钟域的频率
        SHELL_LOG_CLK_INFO("HCLK:  %lu Hz (%.1f MHz)", HAL_RCC_GetHCLKFreq(), HAL_RCC_GetHCLKFreq() / 1000000.0f);
        SHELL_LOG_CLK_INFO("PCLK1: %lu Hz (%.1f MHz)", HAL_RCC_GetPCLK1Freq(), HAL_RCC_GetPCLK1Freq() / 1000000.0f);
        SHELL_LOG_CLK_INFO("PCLK2: %lu Hz (%.1f MHz)", HAL_RCC_GetPCLK2Freq(), HAL_RCC_GetPCLK2Freq() / 1000000.0f);
    } else {
        SHELL_LOG_CLK_ERROR("Clock switch to profile %d failed", profile);
        SHELL_LOG_CLK_ERROR("Clock switch failed!");
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 setclock, cmd_setclock, switch system clock profile);

/* 版本信息命令 */
int cmd_version(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("Version information requested");
    
    SHELL_LOG_SYS_INFO("=== Firmware Version Information ===");
    SHELL_LOG_SYS_INFO("Shell Version: %s", SHELL_VERSION);
    SHELL_LOG_SYS_INFO("HAL Version: %lu", HAL_GetHalVersion());
    SHELL_LOG_SYS_INFO("FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);
    SHELL_LOG_SYS_INFO("Build Date: %s %s", __DATE__, __TIME__);
    SHELL_LOG_SYS_INFO("MCU: STM32H725AEIX");
    
    // 记录版本信息到日志
    SHELL_LOG_SYS_DEBUG("Shell: %s, HAL: %lu, Build: %s %s", 
                        SHELL_VERSION, HAL_GetHalVersion(), __DATE__, __TIME__);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 version, cmd_version, show version information);

/* 十六进制dump命令 */
int cmd_hexdump(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_WARNING("Hexdump command called with insufficient parameters");
        SHELL_LOG_USER_INFO("Usage: hexdump <address> <length>");
        SHELL_LOG_USER_INFO("Example: hexdump 0x08000000 256");
        return -1;
    }
    
    uint32_t addr = strtoul(argv[1], NULL, 0);
    uint32_t len = strtoul(argv[2], NULL, 0);
    
    if (len > 1024) {
        SHELL_LOG_USER_ERROR("Hexdump length too large: %lu bytes (max 1024)", len);
        SHELL_LOG_USER_ERROR("Length too large, max 1024 bytes");
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Hexdump requested: addr=0x%08lX, len=%lu", addr, len);
    SHELL_LOG_USER_INFO("Hex dump from 0x%08lX, length %lu:", addr, len);
    
    uint8_t *ptr = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i += 16) {
        char hex_line[128];
        char ascii_line[20];
        int hex_pos = 0;
        int ascii_pos = 0;
        
        // 格式化地址
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%08lX: ", addr + i);
        
        // 打印十六进制
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%02X ", ptr[i + j]);
        }
        
        // 补齐空格
        for (uint32_t j = len - i; j < 16; j++) {
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "   ");
        }
        
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, " |");
        
        // 打印ASCII
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            char c = ptr[i + j];
            ascii_pos += snprintf(ascii_line + ascii_pos, sizeof(ascii_line) - ascii_pos, 
                                "%c", (c >= 32 && c <= 126) ? c : '.');
        }
        
        ascii_line[ascii_pos] = '\0';
        SHELL_LOG_USER_INFO("%s%s|", hex_line, ascii_line);
    }
    
    SHELL_LOG_USER_DEBUG("Hexdump completed successfully");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 hexdump, cmd_hexdump, hex dump memory);

/* 日志控制命令 */
int cmd_logctl(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: logctl <command> [args...]");
        SHELL_LOG_USER_INFO("Commands:");
        SHELL_LOG_USER_INFO("  status                    - Show current log configuration");
        SHELL_LOG_USER_INFO("  level <global_level>      - Set global log level (0-4)");
        SHELL_LOG_USER_INFO("  module <mod> <level>      - Set module log level");
        SHELL_LOG_USER_INFO("  color <on|off>            - Enable/disable color output");
        SHELL_LOG_USER_INFO("  timestamp <on|off>        - Enable/disable timestamp");
        SHELL_LOG_USER_INFO("  test                      - Test all log levels");
        SHELL_LOG_USER_INFO("");
        SHELL_LOG_USER_INFO("Log Levels: 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=NONE");
        SHELL_LOG_USER_INFO("Modules: 0=SYS, 1=CLK, 2=MEM, 3=TASK, 4=UART, 5=FATFS, 6=USER");
        return 0;
    }
    
    if (strcmp(argv[1], "status") == 0) {
        SHELL_LOG_USER_INFO("=== Log Configuration Status ===");
        SHELL_LOG_USER_INFO("Global Level: %d (%s)", 
                  g_shell_log_config.global_level, 
                  shellLogGetLevelName(g_shell_log_config.global_level));
        SHELL_LOG_USER_INFO("Color Enabled: %s", 
                  g_shell_log_config.color_enabled ? "Yes" : "No");
        SHELL_LOG_USER_INFO("Timestamp Enabled: %s", 
                  g_shell_log_config.timestamp_enabled ? "Yes" : "No");
        SHELL_LOG_USER_INFO("");
        SHELL_LOG_USER_INFO("Module Levels:");
        for (int i = 0; i < SHELL_LOG_MODULE_MAX; i++) {
            SHELL_LOG_USER_INFO("  %s: %d (%s)", 
                      shellLogGetModuleName(i),
                      g_shell_log_config.module_levels[i],
                      shellLogGetLevelName(g_shell_log_config.module_levels[i]));
        }
    }
    else if (strcmp(argv[1], "level") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl level <0-4>");
            return -1;
        }
        int level = atoi(argv[2]);
        if (level < 0 || level > 4) {
            SHELL_LOG_USER_ERROR("Error: Invalid level %d. Valid range: 0-4", level);
            return -1;
        }
        shellLogSetGlobalLevel((ShellLogLevel_t)level);
        SHELL_LOG_USER_INFO("Global log level set to %d (%s)", 
                  level, shellLogGetLevelName((ShellLogLevel_t)level));
    }
    else if (strcmp(argv[1], "module") == 0) {
        if (argc < 4) {
            SHELL_LOG_USER_ERROR("Usage: logctl module <module_id> <level>");
            return -1;
        }
        int module = atoi(argv[2]);
        int level = atoi(argv[3]);
        if (module < 0 || module >= SHELL_LOG_MODULE_MAX) {
            SHELL_LOG_USER_ERROR("Error: Invalid module %d. Valid range: 0-%d", 
                      module, SHELL_LOG_MODULE_MAX - 1);
            return -1;
        }
        if (level < 0 || level > 4) {
            SHELL_LOG_USER_ERROR("Error: Invalid level %d. Valid range: 0-4", level);
            return -1;
        }
        shellLogSetModuleLevel((ShellLogModule_t)module, (ShellLogLevel_t)level);
        SHELL_LOG_USER_INFO("Module %s log level set to %d (%s)", 
                  shellLogGetModuleName((ShellLogModule_t)module),
                  level, shellLogGetLevelName((ShellLogLevel_t)level));
    }
    else if (strcmp(argv[1], "color") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl color <on|off>");
            return -1;
        }
        uint8_t enable = (strcmp(argv[2], "on") == 0) ? 1 : 0;
        shellLogSetColorEnabled(enable);
        SHELL_LOG_USER_INFO("Color output %s", enable ? "enabled" : "disabled");
    }
    else if (strcmp(argv[1], "timestamp") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl timestamp <on|off>");
            return -1;
        }
        uint8_t enable = (strcmp(argv[2], "on") == 0) ? 1 : 0;
        shellLogSetTimestampEnabled(enable);
        SHELL_LOG_USER_INFO("Timestamp output %s", enable ? "enabled" : "disabled");
    }
    else if (strcmp(argv[1], "test") == 0) {
        SHELL_LOG_USER_INFO("Testing all log levels for all modules:");
        for (int module = 0; module < SHELL_LOG_MODULE_MAX; module++) {
            SHELL_LOG_DEBUG((ShellLogModule_t)module, "Debug message from %s module", 
                           shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_INFO((ShellLogModule_t)module, "Info message from %s module", 
                          shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_WARNING((ShellLogModule_t)module, "Warning message from %s module", 
                             shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_ERROR((ShellLogModule_t)module, "Error message from %s module", 
                           shellLogGetModuleName((ShellLogModule_t)module));
        }
        SHELL_LOG_USER_INFO("Log test completed");
    }
    else {
        SHELL_LOG_USER_ERROR("Unknown command: %s", argv[1]);
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 logctl, cmd_logctl, log control and configuration);

/* 日志测试命令 */
int cmd_logtest(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: logtest <module_id>");
        SHELL_LOG_USER_INFO("Modules: 0=SYS, 1=CLK, 2=MEM, 3=TASK, 4=UART, 5=FATFS, 6=USER");
        return -1;
    }
    
    int module = atoi(argv[1]);
    if (module < 0 || module >= SHELL_LOG_MODULE_MAX) {
        SHELL_LOG_USER_ERROR("Error: Invalid module %d. Valid range: 0-%d", 
                  module, SHELL_LOG_MODULE_MAX - 1);
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Testing log output for module %s:", 
              shellLogGetModuleName((ShellLogModule_t)module));
    
    SHELL_LOG_DEBUG((ShellLogModule_t)module, "This is a DEBUG message");
    SHELL_LOG_INFO((ShellLogModule_t)module, "This is an INFO message");
    SHELL_LOG_WARNING((ShellLogModule_t)module, "This is a WARNING message");
    SHELL_LOG_ERROR((ShellLogModule_t)module, "This is an ERROR message");
    
    SHELL_LOG_USER_INFO("Log test completed for module %s", 
              shellLogGetModuleName((ShellLogModule_t)module));
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 logtest, cmd_logtest, test log output for specific module);

/* 变量导出示例 */
static int test_var = 12345;
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_INT), 
                 testVar, &test_var, test variable);

static char test_string[] = "Hello STM32H725!";
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_STRING), 
                 testStr, test_string, test string variable);