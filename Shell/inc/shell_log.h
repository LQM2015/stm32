/**
 * @file shell_log.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell colorful logging system with module control
 * @version 1.0.0
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#ifndef __SHELL_LOG_H__
#define __SHELL_LOG_H__

#include "shell.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ANSI Color Codes */
#define SHELL_COLOR_RESET       "\033[0m"
#define SHELL_COLOR_BLACK       "\033[30m"
#define SHELL_COLOR_RED         "\033[31m"
#define SHELL_COLOR_GREEN       "\033[32m"
#define SHELL_COLOR_YELLOW      "\033[33m"
#define SHELL_COLOR_BLUE        "\033[34m"
#define SHELL_COLOR_MAGENTA     "\033[35m"
#define SHELL_COLOR_CYAN        "\033[36m"
#define SHELL_COLOR_WHITE       "\033[37m"

/* Bright Colors */
#define SHELL_COLOR_BRIGHT_BLACK    "\033[90m"
#define SHELL_COLOR_BRIGHT_RED      "\033[91m"
#define SHELL_COLOR_BRIGHT_GREEN    "\033[92m"
#define SHELL_COLOR_BRIGHT_YELLOW   "\033[93m"
#define SHELL_COLOR_BRIGHT_BLUE     "\033[94m"
#define SHELL_COLOR_BRIGHT_MAGENTA  "\033[95m"
#define SHELL_COLOR_BRIGHT_CYAN     "\033[96m"
#define SHELL_COLOR_BRIGHT_WHITE    "\033[97m"

/* Background Colors */
#define SHELL_COLOR_BG_RED      "\033[41m"
#define SHELL_COLOR_BG_GREEN    "\033[42m"
#define SHELL_COLOR_BG_YELLOW   "\033[43m"
#define SHELL_COLOR_BG_BLUE     "\033[44m"

/* Text Styles */
#define SHELL_STYLE_BOLD        "\033[1m"
#define SHELL_STYLE_DIM         "\033[2m"
#define SHELL_STYLE_UNDERLINE   "\033[4m"
#define SHELL_STYLE_BLINK       "\033[5m"
#define SHELL_STYLE_REVERSE     "\033[7m"

/* Log Level Colors */
#define SHELL_LOG_COLOR_DEBUG   SHELL_COLOR_GREEN
#define SHELL_LOG_COLOR_INFO    SHELL_COLOR_BLUE
#define SHELL_LOG_COLOR_WARNING SHELL_COLOR_BRIGHT_YELLOW
#define SHELL_LOG_COLOR_ERROR   SHELL_COLOR_BRIGHT_RED

/**
 * @brief Log levels
 */
typedef enum {
    SHELL_LOG_LEVEL_DEBUG = 0,    /*!< Debug level */
    SHELL_LOG_LEVEL_INFO,         /*!< Info level */
    SHELL_LOG_LEVEL_WARNING,      /*!< Warning level */
    SHELL_LOG_LEVEL_ERROR,        /*!< Error level */
    SHELL_LOG_LEVEL_NONE          /*!< No logging */
} ShellLogLevel_t;

/**
 * @brief Log modules
 */
typedef enum {
    SHELL_LOG_MODULE_SYSTEM = 0,  /*!< System module */
    SHELL_LOG_MODULE_CLOCK,       /*!< Clock management module */
    SHELL_LOG_MODULE_MEMORY,      /*!< Memory management module */
    SHELL_LOG_MODULE_TASK,        /*!< Task management module */
    SHELL_LOG_MODULE_UART,        /*!< UART communication module */
    SHELL_LOG_MODULE_FATFS,       /*!< File system module */
    SHELL_LOG_MODULE_USER,        /*!< User application module */
    SHELL_LOG_MODULE_MAX          /*!< Maximum module count */
} ShellLogModule_t;

/**
 * @brief Log configuration structure
 */
typedef struct {
    ShellLogLevel_t global_level;                           /*!< Global log level */
    ShellLogLevel_t module_levels[SHELL_LOG_MODULE_MAX];    /*!< Per-module log levels */
    uint8_t color_enabled;                                  /*!< Color output enabled */
    uint8_t timestamp_enabled;                              /*!< Timestamp output enabled */
} ShellLogConfig_t;

/* Global log configuration */
extern ShellLogConfig_t g_shell_log_config;

/**
 * @brief Initialize shell logging system
 */
void shellLogInit(void);

/**
 * @brief Set global log level
 * @param level Log level
 */
void shellLogSetGlobalLevel(ShellLogLevel_t level);

/**
 * @brief Set module log level
 * @param module Module ID
 * @param level Log level
 */
void shellLogSetModuleLevel(ShellLogModule_t module, ShellLogLevel_t level);

/**
 * @brief Enable/disable color output
 * @param enable 1 to enable, 0 to disable
 */
void shellLogSetColorEnabled(uint8_t enable);

/**
 * @brief Enable/disable timestamp output
 * @param enable 1 to enable, 0 to disable
 */
void shellLogSetTimestampEnabled(uint8_t enable);

/**
 * @brief Get module name string
 * @param module Module ID
 * @return Module name string
 */
const char* shellLogGetModuleName(ShellLogModule_t module);

/**
 * @brief Get level name string
 * @param level Log level
 * @return Level name string
 */
const char* shellLogGetLevelName(ShellLogLevel_t level);

/**
 * @brief Get level color string
 * @param level Log level
 * @return Color string
 */
const char* shellLogGetLevelColor(ShellLogLevel_t level);

/**
 * @brief Core logging function
 * @param module Module ID
 * @param level Log level
 * @param format Format string
 * @param ... Arguments
 */
void shellLogPrint(ShellLogModule_t module, ShellLogLevel_t level, const char* format, ...);

/* Convenience macros for different log levels */
#define SHELL_LOG_DEBUG(module, format, ...)   shellLogPrint(module, SHELL_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define SHELL_LOG_INFO(module, format, ...)    shellLogPrint(module, SHELL_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define SHELL_LOG_WARNING(module, format, ...) shellLogPrint(module, SHELL_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define SHELL_LOG_ERROR(module, format, ...)   shellLogPrint(module, SHELL_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

/* Module-specific convenience macros */
#define SHELL_LOG_SYS_DEBUG(format, ...)       SHELL_LOG_DEBUG(SHELL_LOG_MODULE_SYSTEM, format, ##__VA_ARGS__)
#define SHELL_LOG_SYS_INFO(format, ...)        SHELL_LOG_INFO(SHELL_LOG_MODULE_SYSTEM, format, ##__VA_ARGS__)
#define SHELL_LOG_SYS_WARNING(format, ...)     SHELL_LOG_WARNING(SHELL_LOG_MODULE_SYSTEM, format, ##__VA_ARGS__)
#define SHELL_LOG_SYS_ERROR(format, ...)       SHELL_LOG_ERROR(SHELL_LOG_MODULE_SYSTEM, format, ##__VA_ARGS__)

#define SHELL_LOG_CLK_DEBUG(format, ...)       SHELL_LOG_DEBUG(SHELL_LOG_MODULE_CLOCK, format, ##__VA_ARGS__)
#define SHELL_LOG_CLK_INFO(format, ...)        SHELL_LOG_INFO(SHELL_LOG_MODULE_CLOCK, format, ##__VA_ARGS__)
#define SHELL_LOG_CLK_WARNING(format, ...)     SHELL_LOG_WARNING(SHELL_LOG_MODULE_CLOCK, format, ##__VA_ARGS__)
#define SHELL_LOG_CLK_ERROR(format, ...)       SHELL_LOG_ERROR(SHELL_LOG_MODULE_CLOCK, format, ##__VA_ARGS__)

#define SHELL_LOG_MEM_DEBUG(format, ...)       SHELL_LOG_DEBUG(SHELL_LOG_MODULE_MEMORY, format, ##__VA_ARGS__)
#define SHELL_LOG_MEM_INFO(format, ...)        SHELL_LOG_INFO(SHELL_LOG_MODULE_MEMORY, format, ##__VA_ARGS__)
#define SHELL_LOG_MEM_WARNING(format, ...)     SHELL_LOG_WARNING(SHELL_LOG_MODULE_MEMORY, format, ##__VA_ARGS__)
#define SHELL_LOG_MEM_ERROR(format, ...)       SHELL_LOG_ERROR(SHELL_LOG_MODULE_MEMORY, format, ##__VA_ARGS__)

#define SHELL_LOG_TASK_DEBUG(format, ...)      SHELL_LOG_DEBUG(SHELL_LOG_MODULE_TASK, format, ##__VA_ARGS__)
#define SHELL_LOG_TASK_INFO(format, ...)       SHELL_LOG_INFO(SHELL_LOG_MODULE_TASK, format, ##__VA_ARGS__)
#define SHELL_LOG_TASK_WARNING(format, ...)    SHELL_LOG_WARNING(SHELL_LOG_MODULE_TASK, format, ##__VA_ARGS__)
#define SHELL_LOG_TASK_ERROR(format, ...)      SHELL_LOG_ERROR(SHELL_LOG_MODULE_TASK, format, ##__VA_ARGS__)

#define SHELL_LOG_UART_DEBUG(format, ...)      SHELL_LOG_DEBUG(SHELL_LOG_MODULE_UART, format, ##__VA_ARGS__)
#define SHELL_LOG_UART_INFO(format, ...)       SHELL_LOG_INFO(SHELL_LOG_MODULE_UART, format, ##__VA_ARGS__)
#define SHELL_LOG_UART_WARNING(format, ...)    SHELL_LOG_WARNING(SHELL_LOG_MODULE_UART, format, ##__VA_ARGS__)
#define SHELL_LOG_UART_ERROR(format, ...)      SHELL_LOG_ERROR(SHELL_LOG_MODULE_UART, format, ##__VA_ARGS__)

#define SHELL_LOG_FATFS_DEBUG(format, ...)     //SHELL_LOG_DEBUG(SHELL_LOG_MODULE_FATFS, format, ##__VA_ARGS__)
#define SHELL_LOG_FATFS_INFO(format, ...)      SHELL_LOG_INFO(SHELL_LOG_MODULE_FATFS, format, ##__VA_ARGS__)
#define SHELL_LOG_FATFS_WARNING(format, ...)   SHELL_LOG_WARNING(SHELL_LOG_MODULE_FATFS, format, ##__VA_ARGS__)
#define SHELL_LOG_FATFS_ERROR(format, ...)     SHELL_LOG_ERROR(SHELL_LOG_MODULE_FATFS, format, ##__VA_ARGS__)

#define SHELL_LOG_USER_DEBUG(format, ...)      SHELL_LOG_DEBUG(SHELL_LOG_MODULE_USER, format, ##__VA_ARGS__)
#define SHELL_LOG_USER_INFO(format, ...)       SHELL_LOG_INFO(SHELL_LOG_MODULE_USER, format, ##__VA_ARGS__)
#define SHELL_LOG_USER_WARNING(format, ...)    SHELL_LOG_WARNING(SHELL_LOG_MODULE_USER, format, ##__VA_ARGS__)
#define SHELL_LOG_USER_ERROR(format, ...)      SHELL_LOG_ERROR(SHELL_LOG_MODULE_USER, format, ##__VA_ARGS__)

/* Alias macros for compatibility (MEMORY is an alias for MEM) */
#define SHELL_LOG_MEMORY_DEBUG(format, ...)    SHELL_LOG_MEM_DEBUG(format, ##__VA_ARGS__)
#define SHELL_LOG_MEMORY_INFO(format, ...)     SHELL_LOG_MEM_INFO(format, ##__VA_ARGS__)
#define SHELL_LOG_MEMORY_WARNING(format, ...)  SHELL_LOG_MEM_WARNING(format, ##__VA_ARGS__)
#define SHELL_LOG_MEMORY_ERROR(format, ...)    SHELL_LOG_MEM_ERROR(format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_LOG_H__ */