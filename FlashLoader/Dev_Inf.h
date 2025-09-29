/**
  ******************************************************************************
  * @file    Dev_Inf.h
  * @author  STM32CubeProgrammer Flash Loader Team
  * @brief   Device information header file for W25Q256 external flash
  ******************************************************************************
  */

#ifndef DEV_INF_H
#define DEV_INF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
#define SECTOR_NUM     8192   // Max Number of Sector types  

typedef struct 
{
  unsigned long SectorSize;       // Specifies the size of the Sector in Bytes
  unsigned long SectorStartAddr;  // Specifies the start address of the sector area 
} SectorInfo;

/* Device type definitions */
#define UNKNOWN_DEVICE    0
#define ONCHIP_FLASH      1
#define EXT_FLASH         2
#define OTP               3
#define NOR_FLASH         4
#define NAND_FLASH        5
#define NOR_PSRAM         6
#define NOR_SRAM          7

typedef struct 
{
  char           DeviceName[100];               // Specifies the device name 
  unsigned short DeviceType;                   // Specifies the device type  
  unsigned long  DeviceStartAddress;           // Specifies the device start address 
  unsigned long  DeviceSize;                   // Specifies the device size in Bytes
  unsigned long  ProgPageSize;                 // Specifies the device programming page size in Bytes
  unsigned char  EraseValue;                   // Specifies the initial content of erased memory
  SectorInfo     SectorInfo[SECTOR_NUM];       // Specifies list of sectors 
} StorageInfo;

/* Exported constants --------------------------------------------------------*/
extern struct StorageInfo const StorageInfo;

#ifdef __cplusplus
}
#endif

#endif /* DEV_INF_H */