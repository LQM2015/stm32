/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    quadspi.c
  * @brief   This file provides code for the configuration
  *          of the QUADSPI instances.
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
#include "quadspi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Only define hqspi for APP mode, in Flash Loader mode it's defined in Loader_Src.c */
#ifndef FLASH_LOADER
QSPI_HandleTypeDef hqspi;
MDMA_HandleTypeDef hmdma_quadspi_fifo_th;
#endif

/* QUADSPI init function */
void MX_QUADSPI_Init(void)
{

  /* USER CODE BEGIN QUADSPI_Init 0 */

  /* USER CODE END QUADSPI_Init 0 */

  /* USER CODE BEGIN QUADSPI_Init 1 */
  
  /* USER CODE END QUADSPI_Init 1 */
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 1;                                    // QSPI clock = HCLK/2 = 120MHz (W25Q256 max 133MHz)
  hqspi.Init.FifoThreshold = 8;                                     // FIFO threshold 
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;      // Sample shifting for high speed
  hqspi.Init.FlashSize = 24;                                        // 2^(24+1) = 32MB for W25Q256
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;       // CS high time between commands
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;                        // Clock mode 0 (CPOL=0, CPHA=0)
  hqspi.Init.FlashID = QSPI_FLASH_ID_1;                            // Use flash 1
  hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;                   // Single flash mode
  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN QUADSPI_Init 2 */
  DEBUG_INFO("QUADSPI initialized:");
  DEBUG_INFO("  Clock Prescaler: %d (QSPI Clock = %lu MHz)", 
             hqspi.Init.ClockPrescaler, HAL_RCC_GetHCLKFreq() / (hqspi.Init.ClockPrescaler + 1) / 1000000);
  DEBUG_INFO("  Flash Size: 2^(%d+1) = %lu MB", hqspi.Init.FlashSize, (1UL << (hqspi.Init.FlashSize + 1)) / (1024*1024));
  DEBUG_INFO("  FIFO Threshold: %d", hqspi.Init.FifoThreshold);
  /* USER CODE END QUADSPI_Init 2 */

}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef* qspiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(qspiHandle->Instance==QUADSPI)
  {
  /* USER CODE BEGIN QUADSPI_MspInit 0 */

  /* USER CODE END QUADSPI_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* QUADSPI clock enable */
    __HAL_RCC_QSPI_CLK_ENABLE();

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    /**QUADSPI GPIO Configuration
    PG6     ------> QUADSPI_BK1_NCS
    PF6     ------> QUADSPI_BK1_IO3
    PF7     ------> QUADSPI_BK1_IO2
    PF8     ------> QUADSPI_BK1_IO0
    PF10     ------> QUADSPI_CLK
    PF9     ------> QUADSPI_BK1_IO1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  // High speed for W25Q256
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  // High speed for W25Q256
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  // High speed for W25Q256
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

#ifndef FLASH_LOADER
    /* QUADSPI MDMA Init - Only for APP mode */
    /* QUADSPI_FIFO_TH Init */
    hmdma_quadspi_fifo_th.Instance = MDMA_Channel0;
    hmdma_quadspi_fifo_th.Init.Request = MDMA_REQUEST_QUADSPI_FIFO_TH;
    hmdma_quadspi_fifo_th.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
    hmdma_quadspi_fifo_th.Init.Priority = MDMA_PRIORITY_VERY_HIGH;
    hmdma_quadspi_fifo_th.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
    hmdma_quadspi_fifo_th.Init.SourceInc = MDMA_SRC_INC_WORD;
    hmdma_quadspi_fifo_th.Init.DestinationInc = MDMA_DEST_INC_DISABLE;
    hmdma_quadspi_fifo_th.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
    hmdma_quadspi_fifo_th.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
    hmdma_quadspi_fifo_th.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
    hmdma_quadspi_fifo_th.Init.BufferTransferLength = 8;
    hmdma_quadspi_fifo_th.Init.SourceBurst = MDMA_SOURCE_BURST_4BEATS;
    hmdma_quadspi_fifo_th.Init.DestBurst = MDMA_DEST_BURST_4BEATS;
    hmdma_quadspi_fifo_th.Init.SourceBlockAddressOffset = 0;
    hmdma_quadspi_fifo_th.Init.DestBlockAddressOffset = 0;
    if (HAL_MDMA_Init(&hmdma_quadspi_fifo_th) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_MDMA_ConfigPostRequestMask(&hmdma_quadspi_fifo_th, 0, 0) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(qspiHandle,hmdma,hmdma_quadspi_fifo_th);

    /* QUADSPI interrupt Init */
    HAL_NVIC_SetPriority(QUADSPI_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
#endif /* !FLASH_LOADER */
  /* USER CODE BEGIN QUADSPI_MspInit 1 */

  /* USER CODE END QUADSPI_MspInit 1 */
  }
}

void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef* qspiHandle)
{

  if(qspiHandle->Instance==QUADSPI)
  {
  /* USER CODE BEGIN QUADSPI_MspDeInit 0 */

  /* USER CODE END QUADSPI_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_QSPI_CLK_DISABLE();

    /**QUADSPI GPIO Configuration
    PG6     ------> QUADSPI_BK1_NCS
    PF6     ------> QUADSPI_BK1_IO3
    PF7     ------> QUADSPI_BK1_IO2
    PF8     ------> QUADSPI_BK1_IO0
    PF10     ------> QUADSPI_CLK
    PF9     ------> QUADSPI_BK1_IO1
    */
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_10
                          |GPIO_PIN_9);

#ifndef FLASH_LOADER
    /* QUADSPI MDMA DeInit - Only for APP mode */
    HAL_MDMA_DeInit(qspiHandle->hmdma);

    /* QUADSPI interrupt Deinit */
    HAL_NVIC_DisableIRQ(QUADSPI_IRQn);
#endif /* !FLASH_LOADER */
  /* USER CODE BEGIN QUADSPI_MspDeInit 1 */

  /* USER CODE END QUADSPI_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
