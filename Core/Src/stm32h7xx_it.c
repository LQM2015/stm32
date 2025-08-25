/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "shell_port.h"
#include "shell_log.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdio.h>
#include <string.h>

// 异常调试信息结构
typedef struct {
    uint32_t r0, r1, r2, r3, r12, lr, pc, psr;
    uint32_t cfsr, hfsr, dfsr, afsr, bfar, mmar;
} ExceptionInfo_t;

// 打印异常信息
void print_exception_info(ExceptionInfo_t *info, const char *exception_name) {
    SHELL_LOG_SYS_ERROR("=== %s EXCEPTION ===", exception_name);
    SHELL_LOG_SYS_ERROR("R0:  0x%08lX  R1:  0x%08lX  R2:  0x%08lX  R3:  0x%08lX", 
           info->r0, info->r1, info->r2, info->r3);
    SHELL_LOG_SYS_ERROR("R12: 0x%08lX  LR:  0x%08lX  PC:  0x%08lX  PSR: 0x%08lX", 
           info->r12, info->lr, info->pc, info->psr);
    SHELL_LOG_SYS_ERROR("CFSR: 0x%08lX  HFSR: 0x%08lX  DFSR: 0x%08lX  AFSR: 0x%08lX", 
           info->cfsr, info->hfsr, info->dfsr, info->afsr);
    SHELL_LOG_SYS_ERROR("BFAR: 0x%08lX  MMAR: 0x%08lX", info->bfar, info->mmar);
    
    // 分析具体错误原因
    if (info->cfsr & 0x0080) SHELL_LOG_SYS_ERROR("- Imprecise data bus error");
    if (info->cfsr & 0x0040) SHELL_LOG_SYS_ERROR("- Precise data bus error");
    if (info->cfsr & 0x0020) SHELL_LOG_SYS_ERROR("- Instruction bus error");
    if (info->cfsr & 0x0010) SHELL_LOG_SYS_ERROR("- Bus fault on unstacking");
    if (info->cfsr & 0x0008) SHELL_LOG_SYS_ERROR("- Bus fault on stacking");
    if (info->cfsr & 0x0002) SHELL_LOG_SYS_ERROR("- Data access violation");
    if (info->cfsr & 0x0001) SHELL_LOG_SYS_ERROR("- Instruction access violation");
    if (info->cfsr & 0x8000) SHELL_LOG_SYS_ERROR("- Divide by zero");
    if (info->cfsr & 0x4000) SHELL_LOG_SYS_ERROR("- Unaligned access");
    if (info->cfsr & 0x0200) SHELL_LOG_SYS_ERROR("- No coprocessor");
    if (info->cfsr & 0x0100) SHELL_LOG_SYS_ERROR("- Invalid PC load");
    
    SHELL_LOG_SYS_ERROR("=========================");
}

// 获取异常信息的汇编函数
void get_exception_info(ExceptionInfo_t *info) __attribute__((naked));
void get_exception_info(ExceptionInfo_t *info) {
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r1, msp\n"
        "mrsne r1, psp\n"
        "ldm r1, {r2, r3, r12, lr}\n"
        "str r2, [r0, #0]\n"   // r0
        "str r3, [r0, #4]\n"   // r1
        "str r12, [r0, #8]\n"  // r2
        "str lr, [r0, #12]\n"  // r3
        "ldr r2, [r1, #16]\n"
        "str r2, [r0, #16]\n"  // r12
        "ldr r2, [r1, #20]\n"
        "str r2, [r0, #20]\n"  // lr
        "ldr r2, [r1, #24]\n"
        "str r2, [r0, #24]\n"  // pc
        "ldr r2, [r1, #28]\n"
        "str r2, [r0, #28]\n"  // psr
        
        // 读取系统控制寄存器
        "ldr r1, =0xE000ED28\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #32]\n"  // cfsr
        "ldr r1, =0xE000ED2C\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #36]\n"  // hfsr
        "ldr r1, =0xE000ED30\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #40]\n"  // dfsr
        "ldr r1, =0xE000ED3C\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #44]\n"  // afsr
        "ldr r1, =0xE000ED38\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #48]\n"  // bfar
        "ldr r1, =0xE000ED34\n"
        "ldr r2, [r1]\n"
        "str r2, [r0, #52]\n"  // mmar
        
        "bx lr\n"
    );
}
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_OTG_HS;
extern DMA_HandleTypeDef hdma_sai4_a;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim1;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  ExceptionInfo_t info;
  get_exception_info(&info);
  print_exception_info(&info, "HARD FAULT");
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  ExceptionInfo_t info;
  get_exception_info(&info);
  print_exception_info(&info, "MEMORY MANAGEMENT FAULT");
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  ExceptionInfo_t info;
  get_exception_info(&info);
  print_exception_info(&info, "BUS FAULT");
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  ExceptionInfo_t info;
  get_exception_info(&info);
  print_exception_info(&info, "USAGE FAULT");
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM1 update interrupt.
  */
void TIM1_UP_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_IRQn 0 */

  /* USER CODE END TIM1_UP_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_IRQn 1 */

  /* USER CODE END TIM1_UP_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */

  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */

  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS End Point 1 Out global interrupt.
  */
void OTG_HS_EP1_OUT_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_EP1_OUT_IRQn 0 */

  /* USER CODE END OTG_HS_EP1_OUT_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_EP1_OUT_IRQn 1 */

  /* USER CODE END OTG_HS_EP1_OUT_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS End Point 1 In global interrupt.
  */
void OTG_HS_EP1_IN_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_EP1_IN_IRQn 0 */

  /* USER CODE END OTG_HS_EP1_IN_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_EP1_IN_IRQn 1 */

  /* USER CODE END OTG_HS_EP1_IN_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS wake-up interrupt through EXTI line.
  */
void OTG_HS_WKUP_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_WKUP_IRQn 0 */

  /* USER CODE END OTG_HS_WKUP_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_WKUP_IRQn 1 */

  /* USER CODE END OTG_HS_WKUP_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go HS global interrupt.
  */
void OTG_HS_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_HS_IRQn 0 */

  /* USER CODE END OTG_HS_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS);
  /* USER CODE BEGIN OTG_HS_IRQn 1 */

  /* USER CODE END OTG_HS_IRQn 1 */
}

/**
  * @brief This function handles BDMA channel0 global interrupt.
  */
void BDMA_Channel0_IRQHandler(void)
{
  /* USER CODE BEGIN BDMA_Channel0_IRQn 0 */

  /* USER CODE END BDMA_Channel0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_sai4_a);
  /* USER CODE BEGIN BDMA_Channel0_IRQn 1 */

  /* USER CODE END BDMA_Channel0_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
