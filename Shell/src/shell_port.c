/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell port for STM32H750 with FreeRTOS (USART1)
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#include "shell_port.h"
#include "shell.h"
#include "shell_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


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
 * @brief shell写字符串
 * 
 * @param data 字符串
 * @param len 长度
 * 
 * @return short 实际写入的字符长度
 */
short shell_write(char *data, unsigned short len)
{
    HAL_UART_Transmit(SHELL_UART, (uint8_t *)data, len, 0xFFFF);
    
    // 检查是否需要重新显示命令行提示符和已输入内容
    // 如果是换行符或回车符，不需要重新显示
    if (len == 1 && (data[0] == '\r' || data[0] == '\n')) {
        return len;
    }
    
    // 检查是否是日志输出（包含换行符）
    for (int i = 0; i < len; i++) {
        if (data[i] == '\r' || data[i] == '\n') {
            // 如果shell不处于活动状态（即不在执行命令）且有命令行输入
            if (!shell.status.isActive && shell.parser.length > 0) {
                // 等待一小段时间，确保日志输出完成
                for (volatile int j = 0; j < 1000; j++);
                
                // 重新显示提示符
                HAL_UART_Transmit(SHELL_UART, (uint8_t *)"\r\n", 2, 0xFFFF);
                HAL_UART_Transmit(SHELL_UART, (uint8_t *)shell.info.user->data.user.name, 
                                 strlen(shell.info.user->data.user.name), 0xFFFF);
                HAL_UART_Transmit(SHELL_UART, (uint8_t *)":/$ ", 4, 0xFFFF);
                
                // 重新显示已输入的内容
                if (shell.parser.length > 0) {
                    HAL_UART_Transmit(SHELL_UART, (uint8_t *)shell.parser.buffer, 
                                     shell.parser.length, 0xFFFF);
                }
            }
            break;
        }
    }
    
    return len;
}

/**
 * @brief shell读字符
 * 
 * @param data 字符缓冲区
 * @param len 读取长度
 * 
 * @return short 实际读取的字符长度
 */
short shell_read(char *data, unsigned short len)
{
    uint8_t byte;
    if (xQueueReceive(shellRxQueue, &byte, 0) == pdTRUE) {
        *data = byte;
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
    if (huart->Instance == USART1) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 将接收到的字符放入队列
        BaseType_t result = xQueueSendFromISR(shellRxQueue, &uart_rx_buffer[0], &xHigherPriorityTaskWoken);
        if (result != pdTRUE) {
            // 队列满了，记录警告但不影响正常运行
            // 注意：在中断中不能使用日志系统，因为它可能会阻塞
        }
        
        // 重新启动接收
        HAL_StatusTypeDef status = HAL_UART_Receive_IT(SHELL_UART, uart_rx_buffer, 1);
        if (status != HAL_OK) {
            // UART接收重启失败，但在中断中无法记录日志
        }
        
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
    
    SHELL_LOG_SYS_INFO("Shell task started successfully");
    SHELL_LOG_SYS_INFO("Shell version: %s", SHELL_VERSION);
    SHELL_LOG_SYS_INFO("Type 'help' to see available commands");
    
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
    // 首先设置 write 函数，这样即使后续初始化失败，日志也能输出
    shell.write = shell_write;
    shell.read = NULL;  // 暂时禁用读取
    shell.lock = NULL;
    shell.unlock = NULL;
    
    // 检查 FreeRTOS 调度器状态
    // 如果调度器还没启动，我们只做基本初始化
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        // 调度器未启动，只初始化基本的 shell 核心
        shellInit(&shell, shellBuffer, SHELL_BUFFER_SIZE);
        // 注意：此时不创建任务和队列，稍后在 shell_start() 中创建
        return;
    }
    
    // 调度器已启动，完整初始化
    // 创建互斥锁
    shellMutex = xSemaphoreCreateRecursiveMutex();
    if (shellMutex == NULL) {
        // 即使互斥锁创建失败，shell.write 已设置，日志仍可输出
        shellInit(&shell, shellBuffer, SHELL_BUFFER_SIZE);
        return;
    }
    
    // 创建接收队列
    shellRxQueue = xQueueCreate(SHELL_RX_QUEUE_SIZE, sizeof(uint8_t));
    if (shellRxQueue == NULL) {
        shellInit(&shell, shellBuffer, SHELL_BUFFER_SIZE);
        return;
    }
    
    // 完整配置shell
    shell.read = shell_read;
    shell.lock = shell_lock;
    shell.unlock = shell_unlock;
    
    // 初始化shell核心
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
}

/**
 * @brief 初始化shell后的日志输出
 */
void shell_init_log_output(void)
{
    // 只有在 shell.write 已设置时才输出日志
    if (shell.write != NULL) {
        SHELL_LOG_SYS_INFO("Shell system initialized successfully");
        SHELL_LOG_SYS_DEBUG("Shell mutex created: %s", shellMutex != NULL ? "yes" : "no");
        SHELL_LOG_SYS_DEBUG("Shell RX queue created: %s", shellRxQueue != NULL ? "yes" : "no");
        SHELL_LOG_SYS_DEBUG("Shell core initialized (buffer size: %d)", SHELL_BUFFER_SIZE);
    }
}

/**
 * @brief 在FreeRTOS调度器启动后完成shell初始化
 * @note 如果 shell_init() 在调度器启动前调用，则需要在调度器启动后调用此函数
 */
void shell_start(void)
{
    // 如果已经完整初始化（有互斥锁和任务），则不需要再次初始化
    if (shellMutex != NULL && shellTaskHandle != NULL) {
        return;
    }
    
    // 确保调度器已启动
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    
    // 创建互斥锁（如果还没创建）
    if (shellMutex == NULL) {
        shellMutex = xSemaphoreCreateRecursiveMutex();
    }
    
    // 创建接收队列（如果还没创建）
    if (shellRxQueue == NULL) {
        shellRxQueue = xQueueCreate(SHELL_RX_QUEUE_SIZE, sizeof(uint8_t));
    }
    
    // 设置完整的shell回调
    if (shellMutex != NULL) {
        shell.lock = shell_lock;
        shell.unlock = shell_unlock;
    }
    if (shellRxQueue != NULL) {
        shell.read = shell_read;
    }
    
    // 创建shell任务（如果还没创建）
    if (shellTaskHandle == NULL) {
        xTaskCreate(
            shell_task_function,
            "ShellTask",
            SHELL_TASK_STACK_SIZE / sizeof(StackType_t),
            &shell,
            SHELL_TASK_PRIORITY,
            &shellTaskHandle
        );
    }
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
        // 使用日志系统替代直接的shellPrint
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        SHELL_LOG_USER_INFO("%s", buffer);
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