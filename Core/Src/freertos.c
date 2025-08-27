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
#include "iwdg.h"
#include "shell_log.h"
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
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for mic2isp */
osThreadId_t mic2ispHandle;
const osThreadAttr_t mic2isp_attributes = {
  .name = "mic2isp",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for watchdogTask */
osThreadId_t watchdogTaskHandle;
const osThreadAttr_t watchdogTask_attributes = {
  .name = "watchdogTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void mic2isp_task(void *argument);
void watchdog_task(void *argument);

extern void MX_USB_DEVICE_Init(void);
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
   
   // 记录栈溢出信息
   SHELL_LOG_SYS_ERROR("========================================");
   SHELL_LOG_SYS_ERROR("    FreeRTOS 栈溢出检测触发!!!");
   SHELL_LOG_SYS_ERROR("========================================");
   SHELL_LOG_SYS_ERROR("故障任务: %s", (char*)pcTaskName);
   SHELL_LOG_SYS_ERROR("任务句柄: 0x%08X", (uint32_t)xTask);
   
   if (xTask != NULL) {
       SHELL_LOG_SYS_ERROR("任务优先级: %u", (unsigned int)uxTaskPriorityGet(xTask));
       SHELL_LOG_SYS_ERROR("任务状态: %u", (unsigned int)eTaskGetState(xTask));
   }
   
   SHELL_LOG_SYS_ERROR("剩余堆空间: %u bytes", (unsigned int)xPortGetFreeHeapSize());
   SHELL_LOG_SYS_ERROR("系统滴答计数: %lu", xTaskGetTickCount());
   
   SHELL_LOG_SYS_ERROR("========================================");
   SHELL_LOG_SYS_ERROR("栈溢出检测到，系统即将停止运行");
   SHELL_LOG_SYS_ERROR("请检查任务栈大小配置或递归深度");
   SHELL_LOG_SYS_ERROR("========================================");
   
   // 确保输出完成
   for(volatile int i = 0; i < 1000000; i++);
   
   // 触发硬故障以获得更详细的系统状态
   // 这样可以进入我们的故障处理器查看更多信息
   __asm volatile("svc #0");  // 触发系统调用异常
   
   // 如果上面的方法不起作用，直接进入死循环
   while(1) {
       __disable_irq();
       __WFE();
   }
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

  /* creation of mic2isp */
  mic2ispHandle = osThreadNew(mic2isp_task, NULL, &mic2isp_attributes);

  /* creation of watchdogTask */
  watchdogTaskHandle = osThreadNew(watchdog_task, NULL, &watchdogTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
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
  
  // 添加调试输出
  SHELL_LOG_SYS_INFO("DefaultTask started");
  
  // 延迟一段时间确保系统完全稳定
  osDelay(2000);
  
  /* Infinite loop */
  for(;;)
  {
    //SHELL_LOG_SYS_DEBUG("DefaultTask heartbeat");
    osDelay(5000);  // 5秒打印一次心跳
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_mic2isp_task */
/**
* @brief Function implementing the mic2isp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_mic2isp_task */
void mic2isp_task(void *argument)
{
  /* USER CODE BEGIN mic2isp_task */
  
  SHELL_LOG_SYS_INFO("Mic2ISP task started");
  
  /* Infinite loop */
  for(;;)
  {
    //SHELL_LOG_SYS_DEBUG("Mic2ISP task heartbeat");
    osDelay(6000);  // 6秒打印一次心跳
  }
  /* USER CODE END mic2isp_task */
}

/* USER CODE BEGIN Header_watchdog_task */
/**
* @brief Function implementing the watchdog thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_watchdog_task */
void watchdog_task(void *argument)
{
  /* USER CODE BEGIN watchdog_task */
  SHELL_LOG_SYS_INFO("Watchdog task started");
  
  // 给系统一些时间完成初始化
  osDelay(5000);
  
  /* Infinite loop */
  for(;;)
  {
    // 使用安全的喂看门狗函数 - 防止系统重启
    // 看门狗超时时间约32.8秒，我们每15秒喂一次狗，留有充足的安全裕量
    IWDG_SafeRefresh();
    
    //SHELL_LOG_SYS_DEBUG("Watchdog fed - system alive");
    
    // 每15秒喂一次看门狗（超时时间约32.8秒，安全系数约2.2）
    osDelay(15000);
  }
  /* USER CODE END watchdog_task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

