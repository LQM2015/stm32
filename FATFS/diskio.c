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
/* USER CODE END Includes */

/* USER CODE BEGIN Definitions */
/* Physical drive mapping */
#define DEV_MMC		0	/* Map MMC/SD card to physical drive 0 */

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* SD card handle (external) */
extern SD_HandleTypeDef hsd1;

/* Timeout for SD operations */
#define SD_TIMEOUT 30000
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
	
	Stat = STA_NOINIT;
	
	/* Check if the SD card is already initialized */
	if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
		Stat &= ~STA_NOINIT;
	} else {
		/* Initialize the SD card */
		if (HAL_SD_Init(&hsd1) == HAL_OK) {
			Stat &= ~STA_NOINIT;
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
	
	if (HAL_SD_ReadBlocks(&hsd1, (uint8_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) == HAL_OK) {
		/* Wait until the read operation is finished */
		while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
		}
		res = RES_OK;
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
	
	if (HAL_SD_WriteBlocks(&hsd1, (uint8_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) == HAL_OK) {
		/* Wait until the write operation is finished */
		while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
		}
		res = RES_OK;
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

/* USER CODE END */
