/**
  ******************************************************************************
  * @file    Loader_Src.h  
  * @author  STM32CubeProgrammer Flash Loader Team
  * @brief   Header file for Flash Loader
  ******************************************************************************
  */

#ifndef LOADER_SRC_H
#define LOADER_SRC_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
int Init(void);
int Write(uint32_t Address, uint32_t Size, uint8_t* buffer);
int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress);
int MassErase(void);
uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t BufferSize, uint32_t missalignement);

/* Private function prototypes -----------------------------------------------*/
static int CSP_QSPI_WritePage(uint8_t* buffer, uint32_t address, uint32_t size);
static int CSP_QSPI_EraseSector(uint32_t address);

/* External function declarations required by Flash Loader */
extern void SystemInit(void);
extern void MX_QUADSPI_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* LOADER_SRC_H */