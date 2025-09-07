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
#include "audio_recorder.h"
#include "shell_log.h"

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
  hsai_BlockA4.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockA4.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA4.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  /* Use a lower FIFO threshold to generate DMA requests earlier and reduce overrun risk.
   * FULL means DMA only triggered when FIFO full (for RX this can increase latency).
   * 1QF (1/4 full) is a good balance for continuous TDM capture at modest bit clocks.
   * For Late Frame Sync issues, use 1QF threshold for better timing tolerance.
   */
  hsai_BlockA4.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
  hsai_BlockA4.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA4.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA4.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA4.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  
  /* Configure active slots based on channel count from audio_recorder.h */
  hsai_BlockA4.SlotInit.SlotActive = SAI_SLOT_ACTIVE_MASK;
  
  if (HAL_SAI_InitProtocol(&hsai_BlockA4, SAI_PCM_SHORT, SAI_PROTOCOL_DATASIZE_16BIT, AUDIO_CHANNELS) != HAL_OK)
  {
    Error_Handler();
  }
  SHELL_LOG_USER_INFO("SAI initialized: %02x SlotActive %d channels, 16-bit, PCM Short", SAI_SLOT_ACTIVE_MASK, AUDIO_CHANNELS);
  /* USER CODE BEGIN SAI4_Init 2 */
  /*
   * Multi-channel TDM configuration (configurable via audio_recorder.h):
   * Current config: AUDIO_CHANNELS channels, AUDIO_BIT_DEPTH-bit, AUDIO_SAMPLE_RATE Hz
   * 
   * For 4-channel: BCLK ≈ 1.024 MHz, FS = 16 kHz, 4 channels, 16-bit each.
   * Formula: 16k * 4 * 16 = 1.024 MHz => Frame length 64 bits.
   * 
   * For 8-channel: BCLK ≈ 2.048 MHz, FS = 16 kHz, 8 channels, 16-bit each.
   * Formula: 16k * 8 * 16 = 2.048 MHz => Frame length 128 bits.
   * 
   * HAL_SAI_InitProtocol(SAI_PCM_SHORT, 16bit, AUDIO_CHANNELS) automatically configures:
   *   FrameLength, ActiveFrameLength, FSOffset, SlotSize, SlotNumber
   * 
   * Active slots are set via SAI_SLOT_ACTIVE_MASK macro based on AUDIO_CHANNELS
   */

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
    HAL_NVIC_SetPriority(SAI4_IRQn, 6, 0);
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
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
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
