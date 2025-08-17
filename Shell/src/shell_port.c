/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell port for STM32H725 with FreeRTOS
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#include "shell_port.h"
#include "shell.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdarg.h>

/* Private variables ---------------------------------------------------------*/
Shell shell;
static char shellBuffer[SHELL_BUFFER_SIZE];
static SemaphoreHandle_t shellMutex;
static QueueHandle_t shellRxQueue;
static TaskHandle_t shellTaskHandle;

/* UART接收缓冲区 */
static uint8_t uart_rx_buffer[1];
static volatile uint8_t uart_rx_flag = 0;

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief shell写函数
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 实际写入的数据长度
 */
static short shell_write(char *data, unsigned short len)
{
    HAL_UART_Transmit(SHELL_UART, (uint8_t *)data, len, 1000);
    return len;
}

/**
 * @brief shell读函数
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 实际读取到的数据长度
 */
static short shell_read(char *data, unsigned short len)
{
    uint8_t ch;
    if (xQueueReceive(shellRxQueue, &ch, 0) == pdTRUE) {
        *data = ch;
        return 1;
    }
    return 0;
}

/**
 * @brief shell上锁
 * 
 * @param shell shell对象
 * 
 * @return int 0
 */
static int shell_lock(Shell *shell)
{
    if (shellMutex != NULL) {
        xSemaphoreTakeRecursive(shellMutex, portMAX_DELAY);
    }
    return 0;
}

/**
 * @brief shell解锁
 * 
 * @param shell shell对象
 * 
 * @return int 0
 */
static int shell_unlock(Shell *shell)
{
    if (shellMutex != NULL) {
        xSemaphoreGiveRecursive(shellMutex);
    }
    return 0;
}

/**
 * @brief UART接收完成回调函数
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == SHELL_UART) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 将接收到的字符放入队列
        xQueueSendFromISR(shellRxQueue, &uart_rx_buffer[0], &xHigherPriorityTaskWoken);
        
        // 重新启动接收
        HAL_UART_Receive_IT(SHELL_UART, uart_rx_buffer, 1);
        
        // 如果有更高优先级任务被唤醒，进行任务切换
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief Shell任务函数
 * 
 * @param argument 任务参数
 */
static void shell_task_function(void *argument)
{
    Shell *shell = (Shell *)argument;
    
    shellPrint(shell, "Shell task started successfully\r\n");
    shellPrint(shell, "Shell version: %s\r\n", SHELL_VERSION);
    shellPrint(shell, "Type 'help' to see available commands\r\n");
    
    // 启动UART中断接收
    HAL_UART_Receive_IT(SHELL_UART, uart_rx_buffer, 1);
    
    while (1) {
        shellTask(shell);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief 初始化shell
 */
void shell_init(void)
{
    // 创建互斥锁
    shellMutex = xSemaphoreCreateRecursiveMutex();
    if (shellMutex == NULL) {
        // 无法输出错误信息，因为shell还未初始化
        return;
    }
    
    // 创建接收队列
    shellRxQueue = xQueueCreate(SHELL_RX_QUEUE_SIZE, sizeof(uint8_t));
    if (shellRxQueue == NULL) {
        return;
    }
    
    // 配置shell
    shell.write = shell_write;
    shell.read = shell_read;
    shell.lock = shell_lock;
    shell.unlock = shell_unlock;
    
    // 初始化shell
    shellInit(&shell, shellBuffer, SHELL_BUFFER_SIZE);
    
    // 创建shell任务
    BaseType_t result = xTaskCreate(
        shell_task_function,
        "ShellTask",
        SHELL_TASK_STACK_SIZE / sizeof(StackType_t),
        &shell,
        SHELL_TASK_PRIORITY,
        &shellTaskHandle
    );
    
    if (result != pdPASS) {
        return;
    }
    
    // 初始化成功信息将在shell任务中输出
}

/**
 * @brief 获取当前shell对象
 * 
 * @return Shell* shell对象指针
 */
Shell* shell_get_instance(void)
{
    return &shell;
}

/**
 * @brief 获取UART接收缓冲区指针
 * 
 * @return uint8_t* 接收缓冲区指针
 */
uint8_t* shell_get_rx_buffer(void)
{
    return uart_rx_buffer;
}

/**
 * @brief shell打印函数（线程安全）
 * 
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
void shell_printf(const char *fmt, ...)
{
    if (shellMutex != NULL) {
        xSemaphoreTakeRecursive(shellMutex, portMAX_DELAY);
        
        va_list args;
        va_start(args, fmt);
        shellPrint(&shell, fmt, args);
        va_end(args);
        
        xSemaphoreGiveRecursive(shellMutex);
    }
}

/**
 * @brief 执行shell命令
 * 
 * @param cmd 命令字符串
 * @return int 执行结果
 */
int shell_exec(const char *cmd)
{
    if (cmd == NULL) {
        return -1;
    }
    
    return shellRun(&shell, cmd);
}