/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug.c
  * @brief          : Debug output functionality implementation
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

/* Private variables ---------------------------------------------------------*/
static char debug_buffer[DEBUG_BUFFER_SIZE];
static osMutexId_t debug_mutex = NULL;

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef debug_uart_transmit(uint8_t *data, uint16_t length);

/**
  * @brief  初始化调试功能
  * @retval None
  */
void debug_init(void)
{
#if DEBUG_ENABLE
    /* 创建调试输出互斥锁，确保多任务环境下的线程安全 */
    const osMutexAttr_t debug_mutex_attr = {
        .name = "DebugMutex"
    };
    debug_mutex = osMutexNew(&debug_mutex_attr);
    
    /* 发送初始化消息 */
    debug_printf("\r\n");
    debug_printf("========================================\r\n");
    debug_printf("    STM32H750 Debug Output System\r\n");
    debug_printf("    Compiled: %s %s\r\n", __DATE__, __TIME__);
    debug_printf("========================================\r\n");
    
    /* 打印系统信息 */
    debug_print_reset_reason();
    debug_print_system_info();
    debug_print_clock_info();
#endif
}

/**
  * @brief  格式化打印调试信息
  * @param  format: 格式字符串
  * @param  ...: 可变参数
  * @retval 打印的字符数
  */
int debug_printf(const char *format, ...)
{
#if DEBUG_ENABLE
    va_list args;
    int len;
    
    /* 获取互斥锁 */
    if (debug_mutex != NULL) {
        osMutexAcquire(debug_mutex, osWaitForever);
    }
    
    /* 格式化字符串 */
    va_start(args, format);
    len = vsnprintf(debug_buffer, DEBUG_BUFFER_SIZE, format, args);
    va_end(args);
    
    /* 限制长度 */
    if (len > DEBUG_BUFFER_SIZE - 1) {
        len = DEBUG_BUFFER_SIZE - 1;
    }
    
    /* 发送到UART */
    debug_uart_transmit((uint8_t*)debug_buffer, len);
    
    /* 释放互斥锁 */
    if (debug_mutex != NULL) {
        osMutexRelease(debug_mutex);
    }
    
    return len;
#else
    return 0;
#endif
}

/**
  * @brief  通过UART发送调试数据
  * @param  data: 要发送的数据指针
  * @param  length: 数据长度
  * @retval HAL状态
  */
static HAL_StatusTypeDef debug_uart_transmit(uint8_t *data, uint16_t length)
{
    /* 使用阻塞发送，确保数据完整传输 */
    return HAL_UART_Transmit(&DEBUG_UART, data, length, HAL_MAX_DELAY);
}

/**
  * @brief  打印系统信息
  * @retval None
  */
void debug_print_system_info(void)
{
#if DEBUG_ENABLE
    uint32_t chip_id;
    uint32_t rev_id;
    
    chip_id = HAL_GetDEVID();
    rev_id = HAL_GetREVID();
    
    DEBUG_INFO("System Information:");
    DEBUG_INFO("  MCU: STM32H750xB");
    DEBUG_INFO("  Device ID: 0x%03X", chip_id);
    DEBUG_INFO("  Revision ID: 0x%04X", rev_id);
    DEBUG_INFO("  HAL Version: 0x%08X", HAL_GetHalVersion());
    DEBUG_INFO("  UID: %08X-%08X-%08X", 
               HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
#endif
}

/**
  * @brief  打印时钟信息
  * @retval None
  */
void debug_print_clock_info(void)
{
#if DEBUG_ENABLE
    uint32_t sysclk = HAL_RCC_GetSysClockFreq();
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    
    DEBUG_INFO("Clock Information:");
    DEBUG_INFO("  SYSCLK: %lu MHz", sysclk / 1000000);
    DEBUG_INFO("  HCLK:   %lu MHz", hclk / 1000000);
    DEBUG_INFO("  PCLK1:  %lu MHz", pclk1 / 1000000);
    DEBUG_INFO("  PCLK2:  %lu MHz", pclk2 / 1000000);
    DEBUG_INFO("  Tick:   %lu Hz", HAL_GetTickFreq());
#endif
}

/**
  * @brief  打印缓冲区内容（十六进制转储）
  * @param  buffer: 缓冲区指针
  * @param  length: 缓冲区长度
  * @param  name: 缓冲区名称
  * @retval None
  */
void debug_dump_buffer(uint8_t *buffer, uint16_t length, const char *name)
{
#if DEBUG_ENABLE
    uint16_t i;
    
    if (buffer == NULL || length == 0) {
        DEBUG_WARN("Buffer %s is NULL or empty", name ? name : "Unknown");
        return;
    }
    
    DEBUG_INFO("Buffer dump: %s (%d bytes)", name ? name : "Unknown", length);
    
    for (i = 0; i < length; i++) {
        if (i % 16 == 0) {
            debug_printf("%04X: ", i);
        }
        
        debug_printf("%02X ", buffer[i]);
        
        if (i % 16 == 15 || i == length - 1) {
            /* 填充空格 */
            if (i % 16 != 15) {
                for (uint16_t j = i % 16; j < 15; j++) {
                    debug_printf("   ");
                }
            }
            
            /* 打印ASCII字符 */
            debug_printf("  |");
            for (uint16_t j = i - (i % 16); j <= i; j++) {
                char c = (buffer[j] >= 32 && buffer[j] <= 126) ? buffer[j] : '.';
                debug_printf("%c", c);
            }
            debug_printf("|\r\n");
        }
    }
#endif
}

/**
  * @brief  获取调试级别字符串
  * @param  level: 调试级别
  * @retval 级别字符串
  */
const char* debug_level_string(int level)
{
    switch (level) {
        case DEBUG_LEVEL_ERROR:   return "ERROR";
        case DEBUG_LEVEL_WARNING: return "WARN ";
        case DEBUG_LEVEL_INFO:    return "INFO ";
        case DEBUG_LEVEL_DEBUG:   return "DEBUG";
        default:                  return "UNKN ";
    }
}

/**
  * @brief  获取HAL状态字符串
  * @param  status: HAL状态
  * @retval 状态字符串
  */
const char* debug_hal_status_string(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:      return "HAL_OK";
        case HAL_ERROR:   return "HAL_ERROR";
        case HAL_BUSY:    return "HAL_BUSY";
        case HAL_TIMEOUT: return "HAL_TIMEOUT";
        default:          return "HAL_UNKNOWN";
    }
}

/**
  * @brief  打印系统重启原因
  * @retval None
  */
void debug_print_reset_reason(void)
{
#if DEBUG_ENABLE
    uint32_t reset_flags = RCC->RSR;
    
    DEBUG_INFO("Reset Reason:");
    
    if (reset_flags & RCC_RSR_PINRSTF) {
        DEBUG_INFO("  - Pin reset");
    }
    if (reset_flags & RCC_RSR_BORRSTF) {
        DEBUG_INFO("  - Brown-out reset");
    }
    if (reset_flags & RCC_RSR_SFTRSTF) {
        DEBUG_INFO("  - Software reset");
    }
    if (reset_flags & RCC_RSR_IWDG1RSTF) {
        DEBUG_INFO("  - Independent watchdog reset");
    }
    if (reset_flags & RCC_RSR_WWDG1RSTF) {
        DEBUG_INFO("  - Window watchdog reset");
    }
    if (reset_flags & RCC_RSR_LPWRRSTF) {
        DEBUG_INFO("  - Low-power reset");
    }
    
    /* 清除重置标志 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
#endif
}

/**
  * @brief  调试功能测试
  * @retval None
  */
void debug_test(void)
{
#if DEBUG_ENABLE
    uint8_t test_buffer[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                            'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    
    DEBUG_INFO("=== Debug System Test ===");
    
    /* 测试不同级别的调试输出 */
    DEBUG_ERROR("This is an ERROR message");
    DEBUG_WARN("This is a WARNING message");
    DEBUG_INFO("This is an INFO message");
    DEBUG_PRINT("This is a DEBUG message");
    
    /* 测试变量打印 */
    int test_var = 12345;
    DEBUG_PRINT_DEC(test_var);
    DEBUG_PRINT_HEX(test_var);
    
    /* 测试缓冲区转储 */
    DEBUG_DUMP_BUFFER(test_buffer, sizeof(test_buffer));
    
    DEBUG_INFO("=== Debug Test Complete ===");
#endif
}

/* printf重定向到调试输出 --------------------------------------------------*/
#if DEBUG_ENABLE
#ifdef __GNUC__
/* GCC编译器 */
int _write(int file, char *ptr, int len)
{
    debug_uart_transmit((uint8_t*)ptr, len);
    return len;
}
#endif

#ifdef __ICCARM__
/* IAR编译器 */
#include <yfuns.h>
size_t __write(int handle, const unsigned char *buf, size_t bufSize)
{
    if (handle == _LLIO_STDOUT || handle == _LLIO_STDERR) {
        debug_uart_transmit((uint8_t*)buf, bufSize);
        return bufSize;
    }
    return _LLIO_ERROR;
}
#endif

#ifdef __CC_ARM
/* Keil MDK-ARM编译器 */
int fputc(int ch, FILE *f)
{
    debug_uart_transmit((uint8_t*)&ch, 1);
    return ch;
}
#endif
#endif /* DEBUG_ENABLE */