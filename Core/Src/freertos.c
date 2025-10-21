/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "shell_log.h"  /* SHELL_LOG_TASK_xxx macros */
#include "fatfs_init.h"
#include "sdmmc.h"
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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 8,  // ✅ 增加到8KB，避免栈溢出
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for fileSystemTask */
osThreadId_t fileSystemTaskHandle;
const osThreadAttr_t fileSystemTask_attributes = {
  .name = "fileSystemTask",
  .stack_size = 1024 * 4,  // 4KB栈空间用于文件系统操作
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartFileSystemTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);
void vApplicationIdleHook(void);
void vApplicationTickHook(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);
void vApplicationDaemonTaskStartupHook(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
   
#if defined(BOOTLOADER)
   /* BOOTLOADER 模式使用 DEBUG_xxx 宏 */
   DEBUG_ERROR("=== STACK OVERFLOW DETECTED ===");
   DEBUG_ERROR("Task Name: %s", pcTaskName ? (char*)pcTaskName : "Unknown");
   DEBUG_ERROR("Task Handle: 0x%08X", (uint32_t)xTask);
   DEBUG_ERROR("Free Heap: %d bytes", (int)xPortGetFreeHeapSize());
#else
   /* APP 模式使用 SHELL_LOG_xxx 宏 */
   SHELL_LOG_TASK_ERROR("=== STACK OVERFLOW DETECTED ===");
   SHELL_LOG_TASK_ERROR("Task Name: %s", pcTaskName ? (char*)pcTaskName : "Unknown");
   SHELL_LOG_TASK_ERROR("Task Handle: 0x%08X", (uint32_t)xTask);
   SHELL_LOG_TASK_ERROR("Free Heap: %d bytes", (int)xPortGetFreeHeapSize());
#endif
   
   /* 系统将会崩溃，调用错误处理函数 */
   Error_Handler();
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
   
#if defined(BOOTLOADER)
   /* BOOTLOADER 模式使用 DEBUG_xxx 宏 */
   DEBUG_ERROR("=== MEMORY ALLOCATION FAILED ===");
   DEBUG_ERROR("Free Heap Size: %d bytes", (int)xPortGetFreeHeapSize());
   DEBUG_ERROR("Minimum Ever Free Heap: %d bytes", (int)xPortGetMinimumEverFreeHeapSize());
#else
   /* APP 模式使用 SHELL_LOG_xxx 宏 */
   SHELL_LOG_TASK_ERROR("=== MEMORY ALLOCATION FAILED ===");
   SHELL_LOG_TASK_ERROR("Free Heap Size: %d bytes", (int)xPortGetFreeHeapSize());
   SHELL_LOG_TASK_ERROR("Minimum Ever Free Heap: %d bytes", (int)xPortGetMinimumEverFreeHeapSize());
#endif
   
   /* 系统将会崩溃，调用错误处理函数 */
   Error_Handler();
}
/* USER CODE END 5 */

/* USER CODE BEGIN DAEMON_TASK_STARTUP_HOOK */
void vApplicationDaemonTaskStartupHook(void)
{
}
/* USER CODE END DAEMON_TASK_STARTUP_HOOK */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
/* place for user code */
}

__weak void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
/* place for user code */
}
/* USER CODE END PREPOSTSLEEP */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* creation of fileSystemTask */
  fileSystemTaskHandle = osThreadNew(StartFileSystemTask, NULL, &fileSystemTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  
  SHELL_LOG_TASK_INFO("DefaultTask: FreeRTOS started (heap: %d bytes)", (int)xPortGetFreeHeapSize());
  
  /* Wait for system stabilization */
  osDelay(100);
  
  //extern void BSP_SDRAM_Performance_Test(void);
  //BSP_SDRAM_Performance_Test();
  
  SHELL_LOG_TASK_INFO("DefaultTask: Initialization complete");
  
  /* Infinite loop */
  //uint32_t loop_count = 0;
  
  for(;;)
  {
   //  loop_count++;
    
   //  /* 每10秒打印一次状态信息 */
   //  if (loop_count % 10 == 0) {
   //    SHELL_LOG_TASK_DEBUG("DefaultTask: Running - Loop: %lu, Free Heap: %d bytes", 
   //               loop_count / 10, (int)xPortGetFreeHeapSize());
   //  }
    
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/**
  * @brief  Function implementing the fileSystemTask thread.
  * @param  argument: Not used
  * @retval None
  */
void StartFileSystemTask(void *argument)
{
  /* USER CODE BEGIN StartFileSystemTask */
  
  SHELL_LOG_TASK_INFO("FileSystemTask: Starting...");
  
  /* Wait a bit to ensure other peripherals are ready */
  osDelay(200);
  
  /* Initialize SDMMC1 in RTOS context */
  SHELL_LOG_TASK_INFO("FileSystemTask: Initializing SD card...");
  MX_SDMMC1_SD_Init();
  SHELL_LOG_TASK_INFO("FileSystemTask: SD card initialization complete");
  
  /* Initialize FatFs (required for DMA/interrupts) */
  SHELL_LOG_TASK_INFO("FileSystemTask: Initializing FatFs...");
  if (fatfs_init() == 0) {
      SHELL_LOG_FATFS_INFO("FileSystemTask: FatFs initialized successfully");
  } else {
      SHELL_LOG_FATFS_ERROR("FileSystemTask: FatFs initialization failed");
  }
  
  SHELL_LOG_TASK_INFO("FileSystemTask: File system ready");
  
  /* Infinite loop - File system management tasks */
  for(;;)
  {
    /* 这里可以添加文件系统相关的周期性任务 */
    /* 例如：
     * - 监控SD卡状态
     * - 定期同步文件系统
     * - 处理文件操作队列
     */
    
    osDelay(5000);  // 每5秒检查一次
  }
  /* USER CODE END StartFileSystemTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

