/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug.h
  * @brief          : Header for debug output functionality
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Exported defines ----------------------------------------------------------*/
#define DEBUG_ENABLE    1    /* 设置为1启用调试输出，设置为0禁用 */
#define DEBUG_UART      huart1  /* 用于调试输出的UART句柄 */
#define DEBUG_BUFFER_SIZE 256   /* 调试输出缓冲区大小 */

/* Debug level definitions */
#define DEBUG_LEVEL_ERROR   0
#define DEBUG_LEVEL_WARNING 1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_DEBUG   3

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_DEBUG  /* 默认调试级别 */
#endif

/* 调试输出宏定义 */
#if DEBUG_ENABLE

#define DEBUG_PRINT(fmt, ...)       debug_printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#define DEBUG_INFO(fmt, ...)        debug_printf("[INFO]  " fmt "\r\n", ##__VA_ARGS__)
#define DEBUG_WARN(fmt, ...)        debug_printf("[WARN]  " fmt "\r\n", ##__VA_ARGS__)
#define DEBUG_ERROR(fmt, ...)       debug_printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)

/* 带级别的调试宏 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
#define DEBUG_PRINT_LEVEL(level, fmt, ...) do { \
    if (level <= DEBUG_LEVEL) { \
        debug_printf("[%s] " fmt "\r\n", debug_level_string(level), ##__VA_ARGS__); \
    } \
} while(0)
#else
#define DEBUG_PRINT_LEVEL(level, fmt, ...)
#endif

/* 函数入口/出口跟踪宏 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
#define DEBUG_FUNCTION_ENTRY()      DEBUG_PRINT(">>> %s", __FUNCTION__)
#define DEBUG_FUNCTION_EXIT()       DEBUG_PRINT("<<< %s", __FUNCTION__)
#define DEBUG_FUNCTION_EXIT_VAL(val) DEBUG_PRINT("<<< %s = %d", __FUNCTION__, (int)(val))
#else
#define DEBUG_FUNCTION_ENTRY()
#define DEBUG_FUNCTION_EXIT()
#define DEBUG_FUNCTION_EXIT_VAL(val)
#endif

/* 变量打印宏 */
#define DEBUG_PRINT_HEX(var)        DEBUG_PRINT("%s = 0x%08X", #var, (unsigned int)(var))
#define DEBUG_PRINT_DEC(var)        DEBUG_PRINT("%s = %d", #var, (int)(var))
#define DEBUG_PRINT_STR(var)        DEBUG_PRINT("%s = %s", #var, (char*)(var))

/* 内存转储宏 */
#define DEBUG_DUMP_BUFFER(buf, len) debug_dump_buffer((uint8_t*)(buf), (len), #buf)

#else
/* 调试功能禁用时，所有调试宏为空 */
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_INFO(fmt, ...)
#define DEBUG_WARN(fmt, ...)
#define DEBUG_ERROR(fmt, ...)
#define DEBUG_PRINT_LEVEL(level, fmt, ...)
#define DEBUG_FUNCTION_ENTRY()
#define DEBUG_FUNCTION_EXIT()
#define DEBUG_FUNCTION_EXIT_VAL(val)
#define DEBUG_PRINT_HEX(var)
#define DEBUG_PRINT_DEC(var)
#define DEBUG_PRINT_STR(var)
#define DEBUG_DUMP_BUFFER(buf, len)
#endif

/* Exported functions prototypes ---------------------------------------------*/
void debug_init(void);
int debug_printf(const char *format, ...);
void debug_print_system_info(void);
void debug_print_clock_info(void);
void debug_dump_buffer(uint8_t *buffer, uint16_t length, const char *name);
const char* debug_level_string(int level);
void debug_test(void);

/* HAL错误代码转字符串 */
const char* debug_hal_status_string(HAL_StatusTypeDef status);

/* 系统重启原因检测 */
void debug_print_reset_reason(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */