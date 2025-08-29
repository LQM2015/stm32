/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : SAI.c
  * Description        : This file provides code for the configuration
  *                      of the SAI instances.
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
#include "sai.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

SAI_HandleTypeDef hsai_BlockA4;
DMA_HandleTypeDef hdma_sai4_a;

/* SAI4 init function */
void MX_SAI4_Init(void)
{

  /* USER CODE BEGIN SAI4_Init 0 */

  /* USER CODE END SAI4_Init 0 */

  /* USER CODE BEGIN SAI4_Init 1 */

  /* USER CODE END SAI4_Init 1 */

  hsai_BlockA4.Instance = SAI4_Block_A;
  hsai_BlockA4.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA4.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockA4.Init.DataSize = SAI_DATASIZE_16;
  hsai_BlockA4.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA4.Init.ClockStrobing = SAI_CLOCKSTROBING_RISINGEDGE;
  hsai_BlockA4.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA4.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA4.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_FULL;
  hsai_BlockA4.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA4.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA4.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA4.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA4.Init.PdmInit.Activation = DISABLE;
  hsai_BlockA4.Init.PdmInit.MicPairsNbr = 0;
  hsai_BlockA4.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockA4.FrameInit.FrameLength = 128;
  hsai_BlockA4.FrameInit.ActiveFrameLength = 1;
  hsai_BlockA4.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockA4.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA4.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockA4.SlotInit.FirstBitOffset = 0;
  hsai_BlockA4.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
  hsai_BlockA4.SlotInit.SlotNumber = 8;
  hsai_BlockA4.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockA4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI4_Init 2 */

  /* USER CODE END SAI4_Init 2 */

}
static uint32_t SAI4_client =0;

void HAL_SAI_MspInit(SAI_HandleTypeDef* saiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
/* SAI4 */
    if(saiHandle->Instance==SAI4_Block_A)
    {
    /* SAI4 clock enable */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI4A;
    PeriphClkInitStruct.Sai4AClockSelection = RCC_SAI4ACLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    if (SAI4_client == 0)
    {
       __HAL_RCC_SAI4_CLK_ENABLE();

    /* Peripheral interrupt init*/
    HAL_NVIC_SetPriority(SAI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SAI4_IRQn);
    }
    SAI4_client ++;

    /**SAI4_A_Block_A GPIO Configuration
    PD11     ------> SAI4_SD_A
    PD13     ------> SAI4_SCK_A
    PD12     ------> SAI4_FS_A
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Peripheral DMA init*/

    hdma_sai4_a.Instance = BDMA_Channel0;
    hdma_sai4_a.Init.Request = BDMA_REQUEST_SAI4_A;
    hdma_sai4_a.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_sai4_a.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai4_a.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai4_a.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai4_a.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_sai4_a.Init.Mode = DMA_CIRCULAR;
    hdma_sai4_a.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_sai4_a) != HAL_OK)
    {
      Error_Handler();
    }

    /* Several peripheral DMA handle pointers point to the same DMA handle.
     Be aware that there is only one channel to perform all the requested DMAs. */
    __HAL_LINKDMA(saiHandle,hdmarx,hdma_sai4_a);
    __HAL_LINKDMA(saiHandle,hdmatx,hdma_sai4_a);
    }
}

void HAL_SAI_MspDeInit(SAI_HandleTypeDef* saiHandle)
{

/* SAI4 */
    if(saiHandle->Instance==SAI4_Block_A)
    {
    SAI4_client --;
    if (SAI4_client == 0)
      {
      /* Peripheral clock disable */
       __HAL_RCC_SAI4_CLK_DISABLE();
      HAL_NVIC_DisableIRQ(SAI4_IRQn);
      }

    /**SAI4_A_Block_A GPIO Configuration
    PD11     ------> SAI4_SD_A
    PD13     ------> SAI4_SCK_A
    PD12     ------> SAI4_FS_A
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_12);

    HAL_DMA_DeInit(saiHandle->hdmarx);
    HAL_DMA_DeInit(saiHandle->hdmatx);
    }
}

/**
  * @}
  */

/**
  * @}
  */
