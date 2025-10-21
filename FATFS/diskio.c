/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs - STM32H750 SD Card              */
/*-----------------------------------------------------------------------*/
/* Implementation for MMC/SD card via SDMMC interface                    */
/* Date: 2025-10-20                                                      */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */

/* USER CODE BEGIN Includes */
#include "sdmmc.h"		/* STM32 SDMMC HAL driver */
#include "string.h"
#include "debug.h"      /* For DEBUG_INFO/DEBUG_ERROR/DEBUG_WARN */
#include "cmsis_os.h"   /* For FreeRTOS semaphore */
#include "core_cm7.h"   /* For SCB cache operations */
/* USER CODE END Includes */

/* USER CODE BEGIN Definitions */
/* Physical drive mapping */
#define DEV_MMC		0	/* Map MMC/SD card to physical drive 0 */

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* SD card handle (external) */
extern SD_HandleTypeDef hsd1;

/* Timeout for SD operations */
#define SD_TIMEOUT 5000  /* 5 seconds timeout (in ms for CMSIS-RTOS2) */

/* FreeRTOS semaphore for DMA transfer completion */
static osSemaphoreId_t sd_semaphore = NULL;

/* DMA transfer status */
static volatile uint8_t sd_read_complete = 0;
static volatile uint8_t sd_write_complete = 0;
static volatile HAL_StatusTypeDef sd_dma_status = HAL_OK;

/* DMA buffer alignment for STM32H7 (32 bytes) */
#define DMA_BUFFER_ALIGNMENT 32

/* DMA-safe aligned buffer for FatFs operations */
/* Must be in D1 SRAM for best DMA performance */
__attribute__((aligned(DMA_BUFFER_ALIGNMENT))) 
static uint8_t dma_buffer[FF_MAX_SS];  /* FF_MAX_SS = 512 bytes typically */

/* USER CODE END Definitions */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	/* USER CODE BEGIN disk_status */
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}
	
	/* Check if SD card is in transfer state */
	if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
		Stat &= ~STA_NOINIT;
	} else {
		Stat = STA_NOINIT;
	}
	
	return Stat;
	/* USER CODE END disk_status */
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	/* USER CODE BEGIN disk_initialize */
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}
	
	/* Create semaphore for DMA operations (only once) */
	if (sd_semaphore == NULL) {
		sd_semaphore = osSemaphoreNew(1, 0, NULL);
		if (sd_semaphore == NULL) {
			DEBUG_ERROR("[diskio] Failed to create semaphore");
			return STA_NOINIT;
		}
	}
	
	Stat = STA_NOINIT;
	
	/* Check if the SD card is already initialized */
	HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
	
	if (state == HAL_SD_CARD_TRANSFER) {
		Stat &= ~STA_NOINIT;
	} else {
		/* Initialize the SD card */
		if (HAL_SD_Init(&hsd1) == HAL_OK) {
			Stat &= ~STA_NOINIT;
		} else {
			DEBUG_ERROR("[diskio] HAL_SD_Init failed");
		}
	}
	
	return Stat;
	/* USER CODE END disk_initialize */
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	/* USER CODE BEGIN disk_read */
	DRESULT res = RES_ERROR;
	
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}
	
	/* Ensure card is ready before read */
	HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
	uint32_t ready_timeout = 0;
	while (card_state != HAL_SD_CARD_TRANSFER && ready_timeout < 10000) {
		card_state = HAL_SD_GetCardState(&hsd1);
		ready_timeout++;
	}
	
	if (card_state != HAL_SD_CARD_TRANSFER) {
		DEBUG_ERROR("[diskio] Card not ready: %d", card_state);
		return RES_NOTRDY;
	}
	
	/* Check if semaphore is ready */
	if (sd_semaphore == NULL) {
		DEBUG_ERROR("[diskio] Semaphore not initialized");
		return RES_ERROR;
	}
	
	/* Use DMA mode with proper cache management */
	uint8_t use_internal_buffer = 0;
	uint8_t *dma_target = (uint8_t*)buff;
	
	/* Check if user buffer is 32-byte aligned */
	if (((uint32_t)buff & (DMA_BUFFER_ALIGNMENT - 1)) != 0) {
		use_internal_buffer = 1;
		dma_target = dma_buffer;
		
		/* For multi-sector with unaligned buffer, we can only handle 1 sector at a time */
		if (count > 1) {
			DEBUG_ERROR("[diskio] Multi-sector unaligned read not supported");
			return RES_PARERR;
		}
	}
	
	/* Clean D-Cache before DMA read (invalidate the cache lines) */
	/* This ensures DMA writes directly to memory without cache interference */
	uint32_t dma_size = count * 512;
	if (use_internal_buffer) {
		dma_size = 512;  /* Only one sector in internal buffer */
	}
	
	/* Invalidate D-Cache before DMA read */
	SCB_InvalidateDCache_by_Addr((uint32_t*)dma_target, dma_size);
	
	sd_read_complete = 0;
	sd_dma_status = HAL_OK;
	
	/* Start DMA transfer */
	HAL_StatusTypeDef hal_status = HAL_SD_ReadBlocks_DMA(&hsd1, dma_target, (uint32_t)sector, count);
	
	if (hal_status == HAL_OK) {
		/* Wait for DMA transfer complete using semaphore */
		osStatus_t sem_status = osSemaphoreAcquire(sd_semaphore, SD_TIMEOUT);
		
		if (sem_status == osOK && sd_dma_status == HAL_OK) {
			/* Invalidate D-Cache again after DMA write to ensure CPU reads fresh data */
			SCB_InvalidateDCache_by_Addr((uint32_t*)dma_target, dma_size);
			
			/* Wait for card to return to transfer state */
			uint32_t timeout = HAL_GetTick() + 1000;
			while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
				if (HAL_GetTick() >= timeout) {
					DEBUG_ERROR("[diskio] disk_read: timeout waiting for TRANSFER state");
					return RES_ERROR;
				}
			}
			
			/* Copy from internal buffer to user buffer if needed */
			if (use_internal_buffer) {
				memcpy(buff, dma_buffer, 512);
			}
			
			res = RES_OK;
		} else {
			DEBUG_ERROR("[diskio] disk_read: DMA timeout (sem=%d, dma=%d, err=0x%08lX)", 
			           sem_status, sd_dma_status, hsd1.ErrorCode);
			res = RES_ERROR;
		}
	} else {
		DEBUG_ERROR("[diskio] disk_read: HAL_SD_ReadBlocks_DMA failed: %d", hal_status);
		res = RES_ERROR;
	}
	
	if (res != RES_OK) {
		/* Try to recover by reinitializing the card */
		if (HAL_SD_Init(&hsd1) == HAL_OK) {
			sd_read_complete = 0;
			sd_dma_status = HAL_OK;
			
			/* Retry with DMA mode */
			SCB_InvalidateDCache_by_Addr((uint32_t*)dma_target, dma_size);
			hal_status = HAL_SD_ReadBlocks_DMA(&hsd1, dma_target, (uint32_t)sector, count);
			
			if (hal_status == HAL_OK) {
				osStatus_t sem_status = osSemaphoreAcquire(sd_semaphore, SD_TIMEOUT);
				if (sem_status == osOK && sd_dma_status == HAL_OK) {
					SCB_InvalidateDCache_by_Addr((uint32_t*)dma_target, dma_size);
					
					uint32_t timeout = HAL_GetTick() + 1000;
					while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
						if (HAL_GetTick() >= timeout) break;
					}
					if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
						if (use_internal_buffer) {
							memcpy(buff, dma_buffer, 512);
						}
						res = RES_OK;
						DEBUG_INFO("[diskio] disk_read: retry after recovery succeeded");
					}
				}
			}
			
			if (res != RES_OK) {
				DEBUG_ERROR("[diskio] disk_read: retry after recovery failed");
			}
		} else {
			DEBUG_ERROR("[diskio] disk_read: card recovery failed");
		}
	}
	
	return res;
	/* USER CODE END disk_read */
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	/* USER CODE BEGIN disk_write */
	DRESULT res = RES_ERROR;
	
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}
	
	DEBUG_INFO("[diskio] disk_write: pdrv=%d, sector=%lu, count=%u", pdrv, sector, count);
	
	/* Check if semaphore is ready */
	if (sd_semaphore == NULL) {
		DEBUG_ERROR("[diskio] disk_write: Semaphore not initialized!");
		return RES_ERROR;
	}
	
	/* Use DMA mode with proper cache management */
	uint8_t use_internal_buffer = 0;
	const uint8_t *dma_source = buff;
	
	/* Check if user buffer is 32-byte aligned */
	if (((uint32_t)buff & (DMA_BUFFER_ALIGNMENT - 1)) != 0) {
		DEBUG_WARN("[diskio] disk_write: Buffer not aligned (0x%08lX), using internal buffer", (uint32_t)buff);
		use_internal_buffer = 1;
		
		/* For multi-sector with unaligned buffer, we can only handle 1 sector at a time */
		if (count > 1) {
			DEBUG_ERROR("[diskio] disk_write: Multi-sector write with unaligned buffer not supported!");
			return RES_PARERR;
		}
		
		/* Copy to aligned buffer */
		memcpy(dma_buffer, buff, 512);
		dma_source = dma_buffer;
	}
	
	/* Clean D-Cache before DMA write to ensure data is written to memory */
	uint32_t dma_size = count * 512;
	if (use_internal_buffer) {
		dma_size = 512;
	}
	
	DEBUG_INFO("[diskio] disk_write: Cleaning D-Cache for address 0x%08lX, size %lu", 
	          (uint32_t)dma_source, dma_size);
	SCB_CleanDCache_by_Addr((uint32_t*)dma_source, dma_size);
	
	sd_write_complete = 0;
	sd_dma_status = HAL_OK;
	
	HAL_StatusTypeDef hal_status = HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t*)dma_source, (uint32_t)sector, count);
	DEBUG_INFO("[diskio] disk_write: HAL_SD_WriteBlocks_DMA started, status=%d", hal_status);
	
	if (hal_status == HAL_OK) {
		/* Wait for DMA transfer complete using semaphore */
		osStatus_t sem_status = osSemaphoreAcquire(sd_semaphore, SD_TIMEOUT);
		
		if (sem_status == osOK && sd_dma_status == HAL_OK) {
			/* Wait for card to return to transfer state */
			uint32_t timeout = HAL_GetTick() + 1000;
			while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
				if (HAL_GetTick() >= timeout) {
					DEBUG_ERROR("[diskio] disk_write: timeout waiting for TRANSFER state");
					return RES_ERROR;
				}
			}
			res = RES_OK;
			DEBUG_INFO("[diskio] disk_write: DMA success");
		} else {
			DEBUG_ERROR("[diskio] disk_write: DMA failed, sem_status=%d, dma_status=%d, ErrorCode=0x%08lX", 
			           sem_status, sd_dma_status, hsd1.ErrorCode);
			res = RES_ERROR;
		}
	} else {
		DEBUG_ERROR("[diskio] disk_write: HAL_SD_WriteBlocks_DMA start failed with status %d", hal_status);
		res = RES_ERROR;
	}
	
	return res;
	/* USER CODE END disk_write */
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	/* USER CODE BEGIN disk_ioctl */
	DRESULT res = RES_ERROR;
	HAL_SD_CardInfoTypeDef CardInfo;
	
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}
	
	switch (cmd) {
	/* Make sure that no pending write process */
	case CTRL_SYNC:
		res = RES_OK;
		break;
	
	/* Get number of sectors on the disk (DWORD) */
	case GET_SECTOR_COUNT:
		HAL_SD_GetCardInfo(&hsd1, &CardInfo);
		*(DWORD*)buff = CardInfo.LogBlockNbr;
		res = RES_OK;
		break;
	
	/* Get R/W sector size (WORD) */
	case GET_SECTOR_SIZE:
		HAL_SD_GetCardInfo(&hsd1, &CardInfo);
		*(WORD*)buff = CardInfo.LogBlockSize;
		res = RES_OK;
		break;
	
	/* Get erase block size in unit of sector (DWORD) */
	case GET_BLOCK_SIZE:
		HAL_SD_GetCardInfo(&hsd1, &CardInfo);
		*(DWORD*)buff = CardInfo.LogBlockSize / 512;
		res = RES_OK;
		break;
	
	/* MMC/SDC specific command */
	case MMC_GET_TYPE:
		HAL_SD_GetCardInfo(&hsd1, &CardInfo);
		*(BYTE*)buff = CardInfo.CardType;
		res = RES_OK;
		break;
	
	case MMC_GET_CSD:
		memcpy(buff, &(hsd1.CSD), 16);
		res = RES_OK;
		break;
	
	case MMC_GET_CID:
		memcpy(buff, &(hsd1.CID), 16);
		res = RES_OK;
		break;
	
	case MMC_GET_OCR:
		*(DWORD*)buff = hsd1.SdCard.CardType;
		res = RES_OK;
		break;
	
	/* TRIM command (for wear leveling on flash media) */
	case CTRL_TRIM:
		/* SD cards support TRIM/ERASE command */
		/* buff points to array of two DWORD values [start sector, end sector] */
		{
			DWORD *range = (DWORD*)buff;
			if (HAL_SD_Erase(&hsd1, range[0], range[1]) == HAL_OK) {
				res = RES_OK;
			} else {
				res = RES_ERROR;
			}
		}
		break;
	
	default:
		res = RES_PARERR;
		break;
	}
	
	return res;
	/* USER CODE END disk_ioctl */
}

/* USER CODE BEGIN: Additional functions */

/*-----------------------------------------------------------------------*/
/* Get current time for FatFs                                            */
/*-----------------------------------------------------------------------*/
/* This function is called by FatFs module to get current time.          */
/* If the system has an RTC, you should implement this function to       */
/* return the current time. Otherwise, it returns a fixed timestamp.     */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0 && FF_FS_NORTC == 0
DWORD get_fattime (void)
{
	/* USER CODE BEGIN get_fattime */
	/* Return a fixed timestamp if RTC is not implemented */
	/* Format: bit31:25 Year(0-127 +1980), bit24:21 Month(1-12), bit20:16 Day(1-31) */
	/*         bit15:11 Hour(0-23), bit10:5 Minute(0-59), bit4:0 Second/2(0-29) */
	
	/* Example: 2025-01-01 00:00:00 */
	return ((DWORD)(2025 - 1980) << 25) /* Year */
	     | ((DWORD)1 << 21)             /* Month */
	     | ((DWORD)1 << 16)             /* Day */
	     | ((DWORD)0 << 11)             /* Hour */
	     | ((DWORD)0 << 5)              /* Minute */
	     | ((DWORD)0 >> 1);             /* Second / 2 */
	
	/* TODO: If you have RTC, implement it like this:
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
	
	return ((DWORD)(sDate.Year + 2000 - 1980) << 25)
	     | ((DWORD)sDate.Month << 21)
	     | ((DWORD)sDate.Date << 16)
	     | ((DWORD)sTime.Hours << 11)
	     | ((DWORD)sTime.Minutes << 5)
	     | ((DWORD)sTime.Seconds >> 1);
	*/
	/* USER CODE END */
}
#endif

/*-----------------------------------------------------------------------*/
/* HAL SD Tx/Rx Complete Callbacks for DMA Mode                         */
/*-----------------------------------------------------------------------*/

/**
  * @brief  Tx Transfer completed callbacks
  * @param  hsd: SD handle
  * @retval None
  */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	if (hsd == &hsd1) {
		sd_write_complete = 1;
		sd_dma_status = HAL_OK;
		/* Release semaphore to unblock waiting task */
		if (sd_semaphore != NULL) {
			osSemaphoreRelease(sd_semaphore);
		}
	}
}

/**
  * @brief  Rx Transfer completed callbacks
  * @param  hsd: SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	if (hsd == &hsd1) {
		sd_read_complete = 1;
		sd_dma_status = HAL_OK;
		
		/* Release semaphore to unblock waiting task */
		if (sd_semaphore != NULL) {
			osSemaphoreRelease(sd_semaphore);
		}
	}
}

/**
  * @brief  SD error callbacks
  * @param  hsd: SD handle
  * @retval None
  */
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
	if (hsd == &hsd1) {
		sd_read_complete = 0;
		sd_write_complete = 0;
		sd_dma_status = HAL_ERROR;
		
		DEBUG_ERROR("[diskio] DMA Error: 0x%08lX", hsd1.ErrorCode);
		
		/* Release semaphore to unblock waiting task */
		if (sd_semaphore != NULL) {
			osSemaphoreRelease(sd_semaphore);
		}
	}
}

/* USER CODE END */
