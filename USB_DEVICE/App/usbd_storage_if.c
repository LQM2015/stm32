/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_storage_if.c
  * @version        : v1.0_Cube
  * @brief          : Memory management layer.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_storage_if.h"

/* USER CODE BEGIN INCLUDE */
#include "ff_gen_drv.h"
#include "diskio.h"
#include "main.h"
#include "shell_log.h"

/* 性能优化: 批量读写统计 */
uint32_t usb_read_count = 0;
uint32_t usb_write_count = 0;
uint32_t usb_single_sector_reads = 0;
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device.
  * @{
  */

/** @defgroup USBD_STORAGE
  * @brief Usb mass storage device module
  * @{
  */

/** @defgroup USBD_STORAGE_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_STORAGE_Private_Defines
  * @brief Private defines.
  * @{
  */

#define STORAGE_LUN_NBR                  1
#define STORAGE_BLK_NBR                  0x10000
#define STORAGE_BLK_SIZ                  0x200

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_STORAGE_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_STORAGE_Private_Variables
  * @brief Private variables.
  * @{
  */

/* USER CODE BEGIN INQUIRY_DATA_HS */
/** USB Mass storage Standard Inquiry Data. */
const int8_t STORAGE_Inquirydata_HS[] = {/* 36 */

  /* LUN 0 */
  0x00,
  0x80,
  0x02,
  0x02,
  (STANDARD_INQUIRY_DATA_LEN - 5),
  0x00,
  0x00,
  0x00,
  'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
  'P', 'r', 'o', 'd', 'u', 'c', 't', ' ', /* Product      : 16 Bytes */
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '0', '.', '0' ,'1'                      /* Version      : 4 Bytes */
};
/* USER CODE END INQUIRY_DATA_HS */

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_STORAGE_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceHS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_STORAGE_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

int8_t STORAGE_Init_HS(uint8_t lun);
int8_t STORAGE_GetCapacity_HS(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
int8_t STORAGE_IsReady_HS(uint8_t lun);
int8_t STORAGE_IsWriteProtected_HS(uint8_t lun);
int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
int8_t STORAGE_Write_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
int8_t STORAGE_GetMaxLun_HS(void);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_StorageTypeDef USBD_Storage_Interface_fops_HS =
{
  STORAGE_Init_HS,
  STORAGE_GetCapacity_HS,
  STORAGE_IsReady_HS,
  STORAGE_IsWriteProtected_HS,
  STORAGE_Read_HS,
  STORAGE_Write_HS,
  STORAGE_GetMaxLun_HS,
  (int8_t *)STORAGE_Inquirydata_HS
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the storage unit (medium).
  * @param  lun: Logical unit number.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_Init_HS(uint8_t lun)
{
  /* USER CODE BEGIN 9 */
  DSTATUS status = disk_initialize(lun);
  SHELL_LOG_FATFS_INFO("USB Storage Init - LUN: %d, Status: %d", lun, status);
  if (status & STA_NOINIT)
  {
    return (USBD_FAIL);
  }
  return (USBD_OK);
  /* USER CODE END 9 */
}

/**
  * @brief  Returns the medium capacity.
  * @param  lun: Logical unit number.
  * @param  block_num: Number of total block number.
  * @param  block_size: Block size.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_GetCapacity_HS(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
  /* USER CODE BEGIN 10 */
  DRESULT res;

  res = disk_ioctl(lun, GET_SECTOR_COUNT, block_num);
  res |= disk_ioctl(lun, GET_SECTOR_SIZE, block_size);

  if(res != RES_OK)
  {
    SHELL_LOG_FATFS_ERROR("USB Storage GetCapacity failed - LUN: %d", lun);
    *block_num  = 0;
    *block_size = 512; /* Default size */
    return (USBD_FAIL);
  }

  /* 性能优化: GetCapacity频繁调用，使用INFO级别减少日志量 */
  SHELL_LOG_FATFS_INFO("USB GetCapacity - Blocks: %lu, Size: %d", *block_num, *block_size);
  return (USBD_OK);
  /* USER CODE END 10 */
}

/**
  * @brief   Checks whether the medium is ready.
  * @param  lun:  Logical unit number.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_IsReady_HS(uint8_t lun)
{
  /* USER CODE BEGIN 11 */
  DSTATUS status = disk_status(lun);
  /* 性能优化: IsReady频繁调用，仅在错误时输出日志 */
  if (status & STA_NOINIT)
  {
    SHELL_LOG_FATFS_WARNING("USB Storage IsReady - LUN: %d, Status: %d", lun, status);
    return (USBD_FAIL);
  }
  return (USBD_OK);
  /* USER CODE END 11 */
}

/**
  * @brief  Checks whether the medium is write protected.
  * @param  lun: Logical unit number.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_IsWriteProtected_HS(uint8_t lun)
{
  /* USER CODE BEGIN 12 */
  /*
   * 根本原因修复: 无论磁盘是否写保护，此函数都应返回USBD_OK。
   * 根据USB MSC规范，返回FAIL会使主机认为设备故障并中止枚举。
   * 写保护状态是通过其他SCSI命令的Sense Code来传递的，而不是通过这个函数的返回值。
   * 此前错误地返回FAIL是导致枚举失败的直接原因。
   */
  /* 性能优化: 写保护检查频繁调用，使用INFO级别 */
  // SHELL_LOG_FATFS_INFO("USB Storage IsWriteProtected check - LUN: %d", lun);
  (void)lun; /* lun is not used in this simplified implementation */
  return (USBD_OK);
  /* USER CODE END 12 */
}

/**
  * @brief  Reads data from the medium.
  * @param  lun: Logical unit number.
  * @param  buf: data buffer.
  * @param  blk_addr: Logical block address.
  * @param  blk_len: Blocks number.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 13 */
  DRESULT res;
  
  /* 性能统计 */
  usb_read_count++;
  if(blk_len == 1) {
    usb_single_sector_reads++;
    /* 单扇区读取：每100次记录一次，减少日志洪水 */
    if(usb_single_sector_reads % 100 == 0) {
      SHELL_LOG_FATFS_INFO("USB Read Progress - Single sector reads: %lu/%lu", 
                          usb_single_sector_reads, usb_read_count);
    }
  } else {
    /* 多扇区读取仍使用DEBUG级别进行详细跟踪 */
    SHELL_LOG_FATFS_DEBUG("USB Read Multi - Addr: 0x%08lX, Len: %d", blk_addr, blk_len);
  }

  /*
   * 修复的缓存管理：RAM_D2现在配置为不可缓存，无需缓存操作
   */
  SHELL_LOG_SYS_INFO("USB Read: LUN=%d, buf=0x%08lX, addr=%lu, len=%d", 
                    lun, (uint32_t)buf, blk_addr, blk_len);
  
  // 在读取前显示缓冲区状态
  SHELL_LOG_SYS_INFO("Buffer before read: [0-3] = %02X %02X %02X %02X", 
                    buf[0], buf[1], buf[2], buf[3]);
  
  SHELL_LOG_SYS_INFO("RAM_D2 region is non-cacheable, no cache operations needed");
  
  /* 执行读取 - 由于RAM_D2不可缓存，DMA直接写入内存，CPU立即可见 */
  SHELL_LOG_SYS_INFO("About to call disk_read...");
  res = disk_read(lun, buf, blk_addr, blk_len);
  SHELL_LOG_SYS_INFO("disk_read returned: %d", res);
  
  // 在读取后显示缓冲区状态
  SHELL_LOG_SYS_INFO("Buffer after read: [0-3] = %02X %02X %02X %02X", 
                    buf[0], buf[1], buf[2], buf[3]);

  if (res == RES_OK)
  {
    SHELL_LOG_SYS_INFO("USB Storage Read successful");
    return (USBD_OK);
  }
  else
  {
    SHELL_LOG_FATFS_ERROR("USB Storage Read failed - LUN: %d, res: %d", lun, res);
    return (USBD_FAIL);
  }
  /* USER CODE END 13 */
}

/**
  * @brief  Writes data into the medium.
  * @param  lun: Logical unit number.
  * @param  buf: data buffer.
  * @param  blk_addr: Logical block address.
  * @param  blk_len: Blocks number.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t STORAGE_Write_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 14 */
  DRESULT res;
  
  /* 性能统计 */
  usb_write_count++;
  if(blk_len == 1) {
    /* 单扇区写入：每50次记录一次 */
    if(usb_write_count % 50 == 0) {
      SHELL_LOG_FATFS_INFO("USB Write Progress - Count: %lu", usb_write_count);
    }
  } else {
    SHELL_LOG_FATFS_DEBUG("USB Write Multi - Addr: 0x%08lX, Len: %d", blk_addr, blk_len);
  }

  /*
   * DMA缓存一致性操作: 写入前确保数据已写回内存
   * 关键修复: 使用实际缓冲区地址和大小，不进行对齐计算
   */
  uint32_t cache_size = blk_len * 512;
  
  /* 在DMA写入前，确保CPU缓存中的数据已写回内存 */
  SCB_CleanDCache_by_Addr((uint32_t*)buf, cache_size);
  
  res = disk_write(lun, buf, blk_addr, blk_len);

  if (res == RES_OK)
  {
    return (USBD_OK);
  }
  else
  {
    SHELL_LOG_FATFS_ERROR("USB Storage Write failed - LUN: %d, res: %d", lun, res);
    return (USBD_FAIL);
  }
  /* USER CODE END 14 */
}

/**
  * @brief  Returns the Max Supported LUNs.
  * @param  None
  * @retval Lun(s) number.
  */
int8_t STORAGE_GetMaxLun_HS(void)
{
  /* USER CODE BEGIN 15 */
  SHELL_LOG_FATFS_DEBUG("USB Storage GetMaxLun");
  return (STORAGE_LUN_NBR - 1);
  /* USER CODE END 15 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */

