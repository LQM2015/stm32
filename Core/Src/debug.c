/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug.c
  * @brief          : Debug and logging functions implementation
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
#include "debug.h"
#include "cmsis_os.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static debug_level_t current_debug_level = DEBUG_LEVEL_DEBUG;
static char debug_buffer[DEBUG_BUFFER_SIZE];
static uint8_t color_enabled = DEBUG_COLOR_ENABLE;

/* Private function prototypes -----------------------------------------------*/
static uint32_t debug_get_timestamp(void);
static const char* debug_get_level_string(debug_level_t level);
static const char* debug_get_level_color(debug_level_t level);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  获取时间戳
  * @retval 时间戳（毫秒）
  */
static uint32_t debug_get_timestamp(void)
{
#if DEBUG_TIMESTAMP_ENABLE
    return HAL_GetTick();
#else
    return 0;
#endif
}

/**
  * @brief  获取调试级别颜色
  * @param  level 调试级别
  * @retval 颜色代码字符串
  */
static const char* debug_get_level_color(debug_level_t level)
{
    if (!color_enabled) {
        return "";
    }
    
    switch(level) {
        case DEBUG_LEVEL_ERROR: return COLOR_RED;
        case DEBUG_LEVEL_WARN:  return COLOR_YELLOW;
        case DEBUG_LEVEL_INFO:  return COLOR_GREEN;
        case DEBUG_LEVEL_DEBUG: return COLOR_CYAN;
        default:                return COLOR_WHITE;
    }
}
/**
  * @brief  获取调试级别字符串
  * @param  level 调试级别
  * @retval 级别字符串
  */
static const char* debug_get_level_string(debug_level_t level)
{
    switch(level) {
        case DEBUG_LEVEL_ERROR: return "ERROR";
        case DEBUG_LEVEL_WARN:  return "WARN ";
        case DEBUG_LEVEL_INFO:  return "INFO ";
        case DEBUG_LEVEL_DEBUG: return "DEBUG";
        default:                return "UNKN ";
    }
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化调试模块
  * @param  None
  * @retval None
  */
void debug_init(void)
{
    // 等待UART准备就绪
    HAL_Delay(100);
    
    // 打印启动横幅
    debug_print_banner("DEBUG SYSTEM");
    
    if (color_enabled) {
        SYSTEM_PRINTF("Color output: " COLOR_BRIGHT_GREEN "ENABLED" COLOR_RESET);
    } else {
        SYSTEM_PRINTF("Color output: DISABLED");
    }
    
    SYSTEM_PRINTF("MCU: STM32H725");
    SYSTEM_PRINTF("HAL Version: %lu", HAL_GetHalVersion());
    SYSTEM_PRINTF("System Clock: %lu Hz", HAL_RCC_GetSysClockFreq());
    SYSTEM_PRINTF("UART Baudrate: 115200");
    SYSTEM_PRINTF("Debug Level: %s", debug_get_level_string(current_debug_level));
    
    debug_print_separator();
    SUCCESS_PRINTF("Debug system initialized successfully!");
}

/**
  * @brief  设置颜色输出开关
  * @param  enable 1-启用颜色，0-禁用颜色
  * @retval None
  */
void debug_set_color_enable(uint8_t enable)
{
    color_enabled = enable;
    if (enable) {
        SUCCESS_PRINTF("Color output enabled");
    } else {
        INFO_PRINTF("Color output disabled");
    }
}

/**
  * @brief  设置调试级别
  * @param  level 调试级别
  * @retval None
  */
void debug_set_level(debug_level_t level)
{
    current_debug_level = level;
    SYSTEM_PRINTF("Debug level changed to: %s", debug_get_level_string(level));
}

/**
  * @brief  格式化调试输出
  * @param  level 调试级别
  * @param  format 格式字符串
  * @param  ... 可变参数
  * @retval None
  */
void debug_printf(debug_level_t level, const char *format, ...)
{
    if (level > current_debug_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    // 构建完整的调试信息
    int len = 0;
    
#if DEBUG_TIMESTAMP_ENABLE
    if (color_enabled) {
        len += snprintf(debug_buffer + len, DEBUG_BUFFER_SIZE - len, 
                       COLOR_GRAY "[%08lu]" COLOR_RESET " ", debug_get_timestamp());
    } else {
        len += snprintf(debug_buffer + len, DEBUG_BUFFER_SIZE - len, 
                       "[%08lu] ", debug_get_timestamp());
    }
#endif
    
    len += vsnprintf(debug_buffer + len, DEBUG_BUFFER_SIZE - len, format, args);
    
    // 添加回车换行
    if (len < DEBUG_BUFFER_SIZE - 2) {
        debug_buffer[len++] = '\r';
        debug_buffer[len++] = '\n';
        debug_buffer[len] = '\0';
    }
    
    // 通过UART发送
    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)debug_buffer, len, 1000);
    
    va_end(args);
}

/**
  * @brief  打印十六进制数据
  * @param  data 数据指针
  * @param  length 数据长度
  * @param  name 数据名称
  * @retval None
  */
void debug_print_hex(const uint8_t *data, uint16_t length, const char *name)
{
    if (!data || length == 0) {
        return;
    }
    
    if (color_enabled) {
        DEBUG_PRINTF(COLOR_BRIGHT_CYAN "%s" COLOR_RESET " [%d bytes]:", name, length);
    } else {
        DEBUG_PRINTF("%s [%d bytes]:", name, length);
    }
    
    for (uint16_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            if (color_enabled) {
                printf(COLOR_GRAY "%04X:" COLOR_RESET " ", i);
            } else {
                printf("%04X: ", i);
            }
        }
        
        if (color_enabled) {
            printf(COLOR_BRIGHT_WHITE "%02X " COLOR_RESET, data[i]);
        } else {
            printf("%02X ", data[i]);
        }
        
        if ((i + 1) % 16 == 0 || i == length - 1) {
            // 打印ASCII字符
            uint16_t start = (i / 16) * 16;
            uint16_t end = i + 1;
            
            // 补齐空格
            for (uint16_t j = end; j % 16 != 0; j++) {
                printf("   ");
            }
            
            if (color_enabled) {
                printf(" " COLOR_GRAY "|" COLOR_RESET);
            } else {
                printf(" |");
            }
            
            for (uint16_t j = start; j < end; j++) {
                char c = (data[j] >= 32 && data[j] <= 126) ? data[j] : '.';
                if (color_enabled) {
                    printf(COLOR_BRIGHT_WHITE "%c" COLOR_RESET, c);
                } else {
                    printf("%c", c);
                }
            }
            
            if (color_enabled) {
                printf(COLOR_GRAY "|" COLOR_RESET "\r\n");
            } else {
                printf("|\r\n");
            }
        }
    }
}

/**
  * @brief  打印系统信息
  * @param  None
  * @retval None
  */
void debug_print_system_info(void)
{
    debug_print_banner("SYSTEM INFORMATION");
    SYSTEM_PRINTF("CPU ID: 0x%08lX", HAL_GetDEVID());
    SYSTEM_PRINTF("CPU Rev: 0x%08lX", HAL_GetREVID());
    SYSTEM_PRINTF("UID: %08lX-%08lX-%08lX", 
                HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    SYSTEM_PRINTF("System Clock: %lu Hz", HAL_RCC_GetSysClockFreq());
    SYSTEM_PRINTF("HCLK: %lu Hz", HAL_RCC_GetHCLKFreq());
    SYSTEM_PRINTF("PCLK1: %lu Hz", HAL_RCC_GetPCLK1Freq());
    SYSTEM_PRINTF("PCLK2: %lu Hz", HAL_RCC_GetPCLK2Freq());
    SYSTEM_PRINTF("Tick: %lu ms", HAL_GetTick());
    debug_print_separator();
}

/**
  * @brief  打印任务信息
  * @param  None
  * @retval None
  */
void debug_print_task_info(void)
{
    debug_print_banner("FREERTOS TASK INFORMATION");
    TASK_PRINTF("Kernel State: %s", 
                (osKernelGetState() == osKernelRunning) ? "Running" : "Not Running");
    TASK_PRINTF("Kernel Tick: %lu", osKernelGetTickCount());
    TASK_PRINTF("Kernel Frequency: %lu Hz", osKernelGetTickFreq());
    
    // 获取任务统计信息（需要在FreeRTOSConfig.h中启用相关配置）
    #if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    char *pcWriteBuffer = pvPortMalloc(1024);
    if (pcWriteBuffer != NULL) {
        vTaskList(pcWriteBuffer);
        TASK_PRINTF("Task List:\r\n%s", pcWriteBuffer);
        vPortFree(pcWriteBuffer);
    }
    #endif
    
    debug_print_separator();
}

/**
  * @brief  打印内存信息
  * @param  None
  * @retval None
  */
void debug_print_memory_info(void)
{
    debug_print_banner("MEMORY INFORMATION");
    
    #if (configUSE_TRACE_FACILITY == 1)
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
    
    MEMORY_PRINTF("Free Heap: %u bytes", free_heap);
    MEMORY_PRINTF("Min Free Heap: %u bytes", min_free_heap);
    MEMORY_PRINTF("Used Heap: %u bytes", configTOTAL_HEAP_SIZE - free_heap);
    MEMORY_PRINTF("Total Heap: %u bytes", configTOTAL_HEAP_SIZE);
    
    // 计算内存使用百分比
    float usage_percent = ((float)(configTOTAL_HEAP_SIZE - free_heap) / configTOTAL_HEAP_SIZE) * 100;
    if (color_enabled) {
        if (usage_percent > 80) {
            MEMORY_PRINTF("Memory Usage: " COLOR_RED "%.1f%%" COLOR_RESET, usage_percent);
        } else if (usage_percent > 60) {
            MEMORY_PRINTF("Memory Usage: " COLOR_YELLOW "%.1f%%" COLOR_RESET, usage_percent);
        } else {
            MEMORY_PRINTF("Memory Usage: " COLOR_GREEN "%.1f%%" COLOR_RESET, usage_percent);
        }
    } else {
        MEMORY_PRINTF("Memory Usage: %.1f%%", usage_percent);
    }
    #endif
    
    debug_print_separator();
}

/**
  * @brief  打印横幅标题
  * @param  title 标题字符串
  * @retval None
  */
void debug_print_banner(const char *title)
{
    const int banner_width = 50;
    int title_len = strlen(title);
    int padding = (banner_width - title_len - 2) / 2;
    
    if (color_enabled) {
        // 打印顶部边框
        printf(COLOR_BRIGHT_BLUE);
        for (int i = 0; i < banner_width; i++) {
            printf("=");
        }
        printf(COLOR_RESET "\r\n");
        
        // 打印标题行
        printf(COLOR_BRIGHT_BLUE "=");
        for (int i = 0; i < padding; i++) {
            printf(" ");
        }
        printf(COLOR_BRIGHT_WHITE "%s", title);
        for (int i = 0; i < banner_width - padding - title_len - 2; i++) {
            printf(" ");
        }
        printf(COLOR_BRIGHT_BLUE "=" COLOR_RESET "\r\n");
        
        // 打印底部边框
        printf(COLOR_BRIGHT_BLUE);
        for (int i = 0; i < banner_width; i++) {
            printf("=");
        }
        printf(COLOR_RESET "\r\n");
    } else {
        // 无颜色版本
        for (int i = 0; i < banner_width; i++) {
            printf("=");
        }
        printf("\r\n=");
        for (int i = 0; i < padding; i++) {
            printf(" ");
        }
        printf("%s", title);
        for (int i = 0; i < banner_width - padding - title_len - 2; i++) {
            printf(" ");
        }
        printf("=\r\n");
        for (int i = 0; i < banner_width; i++) {
            printf("=");
        }
        printf("\r\n");
    }
}

/**
  * @brief  打印分隔线
  * @param  None
  * @retval None
  */
void debug_print_separator(void)
{
    if (color_enabled) {
        printf(COLOR_GRAY);
        for (int i = 0; i < 50; i++) {
            printf("-");
        }
        printf(COLOR_RESET "\r\n");
    } else {
        for (int i = 0; i < 50; i++) {
            printf("-");
        }
        printf("\r\n");
    }
}

/**
  * @brief  重定向printf到UART
  * @param  file 文件描述符
  * @param  ptr 数据指针
  * @param  len 数据长度
  * @retval 实际发送的字节数
  */
int _write(int file, char *ptr, int len)
{
    if (file == 1 || file == 2) { // stdout or stderr
        HAL_UART_Transmit(DEBUG_UART, (uint8_t*)ptr, len, 1000);
        return len;
    }
    return -1;
}
