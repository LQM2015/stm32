/**
  ******************************************************************************
  * @file    Dev_Inf.c
  * @author  STM32CubeProgrammer Flash Loader Team  
  * @brief   Device information file for W25Q256 external flash
  ******************************************************************************
  */

#include "Dev_Inf.h"

/* This structure contains information used by ST-LINK Utility to program the device */
#if defined (__ICCARM__)
__root struct StorageInfo const StorageInfo  =  {
#else
struct StorageInfo const StorageInfo  =  {
#endif
    "W25Q256_STM32H750-DISCO",                // Device Name  
    NOR_FLASH,                                // Device Type   
    0x90000000,                               // Device Start Address for memory mapped mode
    0x02000000,                               // Device Size in Bytes (32MB)
    0x100,                                    // Programming Page Size 256 Bytes
    0xFF,                                     // Initial Content of Erased Memory
    // Specify Size and Address of Sectors (view example below)
    {
        {0x00001000, 0x00000000},             // Sector Num : 8192 (0x2000), Sector Size: 4KBytes
        {0x00000000, 0x00000000}              // End
    }
};