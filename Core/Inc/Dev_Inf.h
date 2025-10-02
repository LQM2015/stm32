#ifndef DEV_INF_H
#define DEV_INF_H

#include <stdint.h>

// 设备类型定义
#define UNKNOWN         0x00
#define RAM             0x01
#define FLASH           0x02
#define EEPROM          0x03
#define ROM             0x04
#define OTP             0x05
#define NOR_FLASH       0x06
#define NAND_FLASH      0x07
#define SRAM            0x08
#define PSRAM           0x09
#define PC_CARD         0x0A
#define SPI_FLASH       0x0B
#define APROM           0x0C
#define DATAFLASH       0x0D
#define CONFIG          0x0E

// 扇区信息结构 - 使用自然对齐（不要pack！）
struct SectorInfo {
    uint32_t SectorSize;    // 扇区大小
    uint32_t SectorCount;   // 扇区数量
};

// 存储设备信息结构 - 使用自然对齐（不要pack！）
// ⚠️ 重要：官方加载器使用自然对齐，DeviceType后有2字节padding！
// DeviceStartAddress将在偏移104而不是102
struct StorageInfo {
    char DeviceName[100];           // 设备名称 (offset 0)
    uint16_t DeviceType;            // 设备类型 (offset 100)
    // [自动2字节padding]               (offset 102-103)
    uint32_t DeviceStartAddress;    // 起始地址 (offset 104) ← 关键！
    uint32_t DeviceSize;            // 设备大小 (offset 108)
    uint32_t PageSize;              // 页大小 (offset 112)
    uint8_t EraseValue;             // 擦除后的值 (offset 116)
    // [自动3字节padding]               (offset 117-119)
    uint32_t PageProgramTime;       // 页编程时间（100us单位） (offset 120)
    uint32_t SectorEraseTime;       // 扇区擦除时间（ms） (offset 124)
    uint32_t ChipEraseTime;         // 芯片擦除时间（ms） (offset 128)
    struct SectorInfo SectorInfo[10]; // 扇区信息数组 (offset 132)
};

// 导出设备信息
extern struct StorageInfo const StorageInfo;

#endif /* DEV_INF_H */
