#include "Dev_Inf.h"

/* This file is only compiled for Flash Loader configuration */
#ifdef FLASH_LOADER

// 设备信息结构体 - 适配STM32H750XBH6 + W25Q256
// IMPORTANT: This structure MUST be placed in .dev_info section for STM32CubeProgrammer
#if defined (__ICCARM__)
__root struct StorageInfo const StorageInfo @ ".dev_info" = {
#elif defined (__GNUC__)
struct StorageInfo const StorageInfo __attribute__((section(".dev_info"))) = {
#else
struct StorageInfo const StorageInfo = {
#endif
    "W25Q256_STM32H750",         // 设备名称
    SPI_FLASH,                   // 设备类型 - 使用SPI_FLASH而不是NOR_FLASH
    0x90000000,                  // 设备起始地址（STM32H750的QSPI Bank1映射地址）
    0x02000000,                  // 设备大小 32MB (256Mbit)
    0x100,                       // 页大小 256字节
    0xFF,                        // 擦除后的值
    10,                          // 页编程时间(ms) - 实际约0.4ms，留余量
    50,                          // 扇区擦除时间(ms) - 实际约45ms
    100000,                      // 芯片擦除时间(ms) - 实际约80s
    
    // 扇区信息（W25Q256支持4KB扇区擦除）
    {
        {0x00001000, 0x00002000},  // 4KB扇区，8192个 (32MB/4KB = 8192)
        {0x00000000, 0x00000000},  // 结束标记
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000},
        {0x00000000, 0x00000000}
    }
};

#endif /* FLASH_LOADER */
