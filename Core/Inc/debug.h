/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug.h
  * @brief          : Header for debug.c file.
  *                   This file contains the debug and logging functions.
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
    DEBUG_LEVEL_ERROR = 0,
    DEBUG_LEVEL_WARN  = 1,
    DEBUG_LEVEL_INFO  = 2,
    DEBUG_LEVEL_DEBUG = 3
} debug_level_t;

/* Exported constants --------------------------------------------------------*/
#define DEBUG_ENABLE           1
#define DEBUG_UART             &huart3
#define DEBUG_BUFFER_SIZE      512
#define DEBUG_TIMESTAMP_ENABLE 1
#define DEBUG_COLOR_ENABLE     1

/* ANSI Color Codes */
#define COLOR_RESET     "\033[0m"
#define COLOR_RED       "\033[31m"      // ERROR
#define COLOR_YELLOW    "\033[33m"      // WARN
#define COLOR_GREEN     "\033[32m"      // INFO
#define COLOR_CYAN      "\033[36m"      // DEBUG
#define COLOR_BLUE      "\033[34m"      // System info
#define COLOR_MAGENTA   "\033[35m"      // Special
#define COLOR_WHITE     "\033[37m"      // Default
#define COLOR_GRAY      "\033[90m"      // Timestamp

/* Bright Colors */
#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"

/* Background Colors */
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"

/* Exported macro ------------------------------------------------------------*/
#if DEBUG_ENABLE
    #if DEBUG_COLOR_ENABLE
        #define DEBUG_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_DEBUG, COLOR_CYAN "[DEBUG]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define INFO_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO,  COLOR_GREEN "[INFO]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define WARN_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_WARN,  COLOR_YELLOW "[WARN]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define ERROR_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_ERROR, COLOR_RED "[ERROR]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        
        #define SUCCESS_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_INFO, COLOR_BRIGHT_GREEN "[SUCCESS]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define SYSTEM_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO, COLOR_BLUE "[SYSTEM]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define TASK_PRINTF(fmt, ...)     debug_printf(DEBUG_LEVEL_INFO, COLOR_MAGENTA "[TASK]" COLOR_RESET " " fmt, ##__VA_ARGS__)
        #define MEMORY_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO, COLOR_BRIGHT_BLUE "[MEMORY]" COLOR_RESET " " fmt, ##__VA_ARGS__)
    #else
        #define DEBUG_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_DEBUG, "[DEBUG] " fmt, ##__VA_ARGS__)
        #define INFO_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO,  "[INFO] " fmt, ##__VA_ARGS__)
        #define WARN_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_WARN,  "[WARN] " fmt, ##__VA_ARGS__)
        #define ERROR_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_ERROR, "[ERROR] " fmt, ##__VA_ARGS__)
        
        #define SUCCESS_PRINTF(fmt, ...)  debug_printf(DEBUG_LEVEL_INFO, "[SUCCESS] " fmt, ##__VA_ARGS__)
        #define SYSTEM_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO, "[SYSTEM] " fmt, ##__VA_ARGS__)
        #define TASK_PRINTF(fmt, ...)     debug_printf(DEBUG_LEVEL_INFO, "[TASK] " fmt, ##__VA_ARGS__)
        #define MEMORY_PRINTF(fmt, ...)   debug_printf(DEBUG_LEVEL_INFO, "[MEMORY] " fmt, ##__VA_ARGS__)
    #endif
    
    #define DEBUG_PRINT_HEX(data, len) debug_print_hex(data, len, "HEX")
    #define DEBUG_PRINT_BUFFER(data, len, name) debug_print_hex(data, len, name)
#else
    #define DEBUG_PRINTF(fmt, ...)
    #define INFO_PRINTF(fmt, ...)
    #define WARN_PRINTF(fmt, ...)
    #define ERROR_PRINTF(fmt, ...)
    #define SUCCESS_PRINTF(fmt, ...)
    #define SYSTEM_PRINTF(fmt, ...)
    #define TASK_PRINTF(fmt, ...)
    #define MEMORY_PRINTF(fmt, ...)
    #define DEBUG_PRINT_HEX(data, len)
    #define DEBUG_PRINT_BUFFER(data, len, name)
#endif

/* Exported functions prototypes ---------------------------------------------*/
void debug_init(void);
void debug_set_level(debug_level_t level);
void debug_set_color_enable(uint8_t enable);
void debug_printf(debug_level_t level, const char *format, ...);
void debug_print_hex(const uint8_t *data, uint16_t length, const char *name);
void debug_print_system_info(void);
void debug_print_task_info(void);
void debug_print_memory_info(void);
void debug_print_banner(const char *title);
void debug_print_separator(void);
int _write(int file, char *ptr, int len);

/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */
