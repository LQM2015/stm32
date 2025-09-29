/**
 * @file    Dev_Inf.c
 * @brief   Device information for STM32CubeProgrammer (.stldr format)
 * @author  STM32CubeProgrammer Team
 * @date    2025-09-29
 */

#include "Dev_Inf_stldr.h"

/* Device information structure - 必须放在固定地址供STM32CubeProgrammer读取 */
#if defined (__ICCARM__)
#pragma location = "DeviceData"
__root const struct StorageInfo StorageInfo = {
#else
__attribute__((section(".devicedata"))) const struct StorageInfo StorageInfo = {
#endif
   "W25Q256_STM32H750XBH6",                 // Device Name (缩短名称避免解析问题)
   NOR_FLASH,                               // Device Type  
   0x90000000,                              // Device Start Address
   0x02000000,                              // Device Size in Bytes (32MB)
   0x00000100,                              // Programming Page Size 256Bytes
   0xFF,                                    // Initial Content of Erased Memory
   // Sectors definition
   0x00002000,                              // Sector Count: 8192 sectors
   0x00001000                               // Sector Size: 4096 Bytes each
};