/**
 * @file    Dev_Inf.h  
 * @brief   Device information header for .stldr format
 * @author  STM32CubeProgrammer Team
 * @date    2025-09-29
 */

#ifndef __DEV_INF_H
#define __DEV_INF_H

#include <stdint.h>

#define LOADER_OK   0x1
#define LOADER_FAIL 0x0

/* Device type definitions */
#define UNKNOWN_FLASH    0
#define ONCHIP_FLASH     1
#define EXT_FLASH        2
#define EXT_SRAM         3
#define NOR_FLASH        4
#define NAND_FLASH       5

/* Storage information structure for STM32CubeProgrammer */
#pragma pack(1)  /* 确保结构体紧密打包，避免字节对齐问题 */
struct StorageInfo {
    char           DeviceName[100];          // Device Name + Version
    uint16_t       DeviceType;               // Device Type: ONCHIP_FLASH, EXT_FLASH, EXT_SRAM, NOR_FLASH, NAND_FLASH
    uint32_t       DeviceStartAddress;       // Default Device Start Address
    uint32_t       DeviceSize;               // Total Size of Device
    uint32_t       PageSize;                 // Programming Page Size
    uint8_t        EraseValue;               // Content of Erased Memory

    // Sectors definition - 使用传统的扇区描述格式
    uint32_t       SectorCount;              // Number of sectors
    uint32_t       SectorSize;               // Size of each sector
};
#pragma pack()   /* 恢复默认对齐 */

#endif /* __DEV_INF_H */