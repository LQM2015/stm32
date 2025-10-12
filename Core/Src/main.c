/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

#if defined(BOOTLOADER)
/* Bootloader mode - only essential includes */
#include "quadspi.h"
#include "usart.h"
#include "gpio.h"
#include "bootloader.h"
#elif !defined(FLASH_LOADER)
/* Normal application mode */
#include "cmsis_os.h"
#include "dma.h"
#include "mdma.h"
#include "quadspi.h"
#include "usart.h"
#include "gpio.h"
#endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

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
void SystemClock_Config(void);
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
#ifdef BOOTLOADER
  /* Bootloader mode - Initialize and jump to external Flash application */
  
  /* Enable I-Cache and D-Cache */
  SCB_EnableICache();
  SCB_EnableDCache();
  
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  
  /* Configure the system clock */
  SystemClock_Config();
  
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_QUADSPI_Init();
  
  /* Initialize bootloader */
  Bootloader_Init();
  Bootloader_PrintInfo();
  
  /* Initialize QSPI Flash */
  DEBUG_INFO("Initializing QSPI Flash...");
  int8_t qspi_status = QSPI_W25Qxx_Init();
  if (qspi_status == QSPI_W25Qxx_OK) {
    DEBUG_INFO("QSPI Flash initialized successfully");
    
    /* Read Flash ID for verification */
    uint32_t flash_id = QSPI_W25Qxx_ReadID();
    DEBUG_INFO("Flash ID: 0x%06X (Expected: 0xEF4019 for W25Q256)", flash_id);
    
    /* Enter memory-mapped mode */
    DEBUG_INFO("Entering memory-mapped mode...");
    if (QSPI_W25Qxx_MemoryMappedMode() == QSPI_W25Qxx_OK) {
      DEBUG_INFO("Memory-mapped mode activated");
      
      /* Verify application */
      if (Bootloader_VerifyApplication()) {
        /* Jump to application */
        Bootloader_JumpToApplication();
        /* Never returns */
      } else {
        DEBUG_ERROR("Application verification failed!");
        DEBUG_ERROR("Cannot jump to application");
      }
    } else {
      DEBUG_ERROR("Failed to enter memory-mapped mode!");
    }
  } else {
    DEBUG_ERROR("QSPI Flash initialization failed!");
  }
  
  /* If we reach here, something went wrong */
  DEBUG_ERROR("Bootloader stuck - cannot proceed");
  while(1) {
    HAL_Delay(1000);
  }
  
#elif defined(FLASH_LOADER)
  /* Flash Loader mode - minimal initialization */
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  
  /* Configure the system clock */
  SystemClock_Config();
  
  /* Flash Loader runs its own functions, main() should not execute normal app code */
  while(1)
  {
    /* Flash Loader functions are called by STM32CubeProgrammer */
    /* This loop should never be reached in normal operation */
  }
#else
  /* Normal application mode */

  /* USER CODE BEGIN 1 */
  /* 极早期调试输出 - 使用直接寄存器操作,不依赖 HAL */
  /* UART1 已经被 bootloader 初始化,可以直接使用 */
  /* 这样即使后续初始化失败也能看到输出 */
  
  /* APP 启动标记 - 使用 DEBUG 宏(需要 UART 工作) */
  /* 但 UART 应该已经被 bootloader 初始化过了 */
  
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_MDMA_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化调试输出功能 */
  DEBUG_INFO("=== APP SUCCESSFULLY STARTED ===");
  DEBUG_INFO("APP is running from external Flash at 0x90000000");
  DEBUG_INFO("Bootloader handoff successful!");
  DEBUG_INFO("Starting main function initialization...");
  
  /* 运行调试测试 */
  debug_test();
  
  DEBUG_INFO("UART1 initialized at 115200 baud");
  DEBUG_INFO("QUADSPI initialized");
  DEBUG_INFO("DMA and MDMA initialized");
  DEBUG_INFO("GPIO initialized");
  DEBUG_INFO("System ready, starting FreeRTOS scheduler...");
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 这里不应该执行到，因为控制权已经交给FreeRTOS调度器 */
    DEBUG_ERROR("ERROR: Unexpected return from FreeRTOS scheduler!");
    HAL_Delay(1000);
  }
  /* USER CODE END 3 */
#endif /* Normal application mode */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

#if !defined(BOOTLOADER)
/**
 * @brief  配置 MPU (Memory Protection Unit)
 * @note   为不同内存区域设置合适的访问权限和缓存策略
 *         提高系统安全性和性能
 */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* 禁用 MPU */
  HAL_MPU_Disable();

  /* ========== Region 0: 外部 QSPI Flash (APP 代码) ========== */
  /* 地址: 0x90000000 - 0x91FFFFFF (32MB)
   * 用途: 存储和执行 APP 代码
   * 配置: 可读可执行,启用缓存以提高性能
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x90000000;          // 外部 Flash 起始地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;       // 32MB W25Q256
  MPU_InitStruct.SubRegionDisable = 0x00;           // 启用所有子区域
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;     // Normal memory
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;        // 可读可写
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;      // 允许执行代码!
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;               // 启用缓存(重要!)
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* ========== Region 1: DTCM RAM (堆栈) ========== */
  /* 地址: 0x20000000 - 0x2001FFFF (128KB)
   * 用途: 主堆栈,快速访问的变量
   * 配置: 可读可写可执行,启用缓存
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x20000000;          // DTCM RAM
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;      // 允许从 RAM 执行
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* ========== Region 2: AXI SRAM (D1 域 - 主 RAM) ========== */
  /* 地址: 0x24000000 - 0x2407FFFF (512KB)
   * 用途: 主要的数据存储区域,DMA 缓冲区
   * 配置: 可读可写可执行,启用缓存
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x24000000;          // AXI SRAM
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* ========== Region 3: 外设区域 ========== */
  /* 地址: 0x40000000 - 0x5FFFFFFF (512MB)
   * 用途: 所有外设寄存器 (UART, GPIO, QSPI, DMA 等)
   * 配置: 可读可写,不可执行,不可缓存
   * 注意: 外设寄存器必须不可缓存!否则会读到旧值
   */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0x40000000;          // 外设基地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_512MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;     // Device memory
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;     // 外设不可执行
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;           // 外设不可缓存!
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 启用 MPU,特权模式下使用默认内存映射 */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
#endif /* !BOOTLOADER */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  
  /* 输出错误信息 */
  DEBUG_ERROR("=== SYSTEM ERROR DETECTED ===");
  DEBUG_ERROR("Error Handler called from: %s", __FILE__);
  DEBUG_ERROR("System will be halted");
  DEBUG_ERROR("Check UART output for more details");
  
  /* 打印当前系统状态 */
  DEBUG_ERROR("Current Tick: %lu", HAL_GetTick());
  DEBUG_ERROR("SYSCLK: %lu Hz", HAL_RCC_GetSysClockFreq());
  
  /* 禁用中断并进入死循环 */
  __disable_irq();
  while (1)
  {
    /* 可以在这里添加LED闪烁等错误指示 */
    HAL_Delay(500);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
#if !defined(FLASH_LOADER) && !defined(BOOTLOADER)
  DEBUG_ERROR("=== ASSERTION FAILED ===");
  DEBUG_ERROR("File: %s", file);
  DEBUG_ERROR("Line: %lu", line);
  DEBUG_ERROR("Check your code parameters!");
  
  /* 调用错误处理函数 */
  Error_Handler();
#else
  /* FLASH_LOADER or BOOTLOADER mode - minimal assert handling */
  (void)file;
  (void)line;
  while(1)
  {
    /* Halt execution on assertion failure */
  }
#endif
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
