/**
 * @file shell_log.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell colorful logging system implementation
 * @version 1.0.0
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#include "shell_log.h"
#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Global log configuration */
ShellLogConfig_t g_shell_log_config = {
    .global_level = SHELL_LOG_LEVEL_DEBUG,
    .module_levels = {
        [SHELL_LOG_MODULE_SYSTEM] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_CLOCK] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_MEMORY] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_TASK] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_UART] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_FATFS] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_AW882XX] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_AUDIO] = SHELL_LOG_LEVEL_DEBUG,
        [SHELL_LOG_MODULE_USER] = SHELL_LOG_LEVEL_DEBUG,
    },
    .color_enabled = 1,
    .timestamp_enabled = 1
};

/* Module name strings */
static const char* module_names[SHELL_LOG_MODULE_MAX] = {
    [SHELL_LOG_MODULE_SYSTEM] = "SYS",
    [SHELL_LOG_MODULE_CLOCK] = "CLK",
    [SHELL_LOG_MODULE_MEMORY] = "MEM",
    [SHELL_LOG_MODULE_TASK] = "TASK",
    [SHELL_LOG_MODULE_UART] = "UART",
    [SHELL_LOG_MODULE_FATFS] = "FATFS",
    [SHELL_LOG_MODULE_AW882XX] = "AW882XX",
    [SHELL_LOG_MODULE_AUDIO] = "AUDIO",
    [SHELL_LOG_MODULE_USER] = "USER",
};

/* Level name strings */
static const char* level_names[SHELL_LOG_LEVEL_NONE] = {
    [SHELL_LOG_LEVEL_DEBUG] = "DEBUG",
    [SHELL_LOG_LEVEL_INFO] = "INFO",
    [SHELL_LOG_LEVEL_WARNING] = "WARN",
    [SHELL_LOG_LEVEL_ERROR] = "ERROR",
};

/* Level color strings */
static const char* level_colors[SHELL_LOG_LEVEL_NONE] = {
    [SHELL_LOG_LEVEL_DEBUG] = SHELL_LOG_COLOR_DEBUG,
    [SHELL_LOG_LEVEL_INFO] = SHELL_LOG_COLOR_INFO,
    [SHELL_LOG_LEVEL_WARNING] = SHELL_LOG_COLOR_WARNING,
    [SHELL_LOG_LEVEL_ERROR] = SHELL_LOG_COLOR_ERROR,
};

/**
 * @brief Initialize shell logging system
 */
void shellLogInit(void)
{
    /* Already initialized with default values */
}

/**
 * @brief Set global log level
 * @param level Log level
 */
void shellLogSetGlobalLevel(ShellLogLevel_t level)
{
    if (level <= SHELL_LOG_LEVEL_NONE) {
        g_shell_log_config.global_level = level;
    }
}

/**
 * @brief Set module log level
 * @param module Module ID
 * @param level Log level
 */
void shellLogSetModuleLevel(ShellLogModule_t module, ShellLogLevel_t level)
{
    if (module < SHELL_LOG_MODULE_MAX && level <= SHELL_LOG_LEVEL_NONE) {
        g_shell_log_config.module_levels[module] = level;
    }
}

/**
 * @brief Enable/disable color output
 * @param enable 1 to enable, 0 to disable
 */
void shellLogSetColorEnabled(uint8_t enable)
{
    g_shell_log_config.color_enabled = enable ? 1 : 0;
}

/**
 * @brief Enable/disable timestamp output
 * @param enable 1 to enable, 0 to disable
 */
void shellLogSetTimestampEnabled(uint8_t enable)
{
    g_shell_log_config.timestamp_enabled = enable ? 1 : 0;
}

/**
 * @brief Get module name string
 * @param module Module ID
 * @return Module name string
 */
const char* shellLogGetModuleName(ShellLogModule_t module)
{
    if (module < SHELL_LOG_MODULE_MAX) {
        return module_names[module];
    }
    return "UNKNOWN";
}

/**
 * @brief Get level name string
 * @param level Log level
 * @return Level name string
 */
const char* shellLogGetLevelName(ShellLogLevel_t level)
{
    if (level < SHELL_LOG_LEVEL_NONE) {
        return level_names[level];
    }
    return "UNKNOWN";
}

/**
 * @brief Get level color string
 * @param level Log level
 * @return Color string
 */
const char* shellLogGetLevelColor(ShellLogLevel_t level)
{
    if (level < SHELL_LOG_LEVEL_NONE) {
        return level_colors[level];
    }
    return SHELL_COLOR_RESET;
}

/**
 * @brief Check if log should be printed
 * @param module Module ID
 * @param level Log level
 * @return 1 if should print, 0 otherwise
 */
static uint8_t shellLogShouldPrint(ShellLogModule_t module, ShellLogLevel_t level)
{
    /* Check global level */
    if (level < g_shell_log_config.global_level) {
        return 0;
    }
    
    /* Check module level */
    if (module < SHELL_LOG_MODULE_MAX && level < g_shell_log_config.module_levels[module]) {
        return 0;
    }
    
    return 1;
}

/**
 * @brief Get any available shell (not necessarily active)
 * @return Shell* First available shell or NULL
 */
static Shell* shellLogGetShell(void)
{
    extern Shell shell;  // 引用shell_port.c中的全局shell对象
    return &shell;
}

/**
 * @brief Core logging function
 * @param module Module ID
 * @param level Log level
 * @param format Format string
 * @param ... Arguments
 */
void shellLogPrint(ShellLogModule_t module, ShellLogLevel_t level, const char* format, ...)
{
    Shell *shell = shellLogGetShell();
    if (!shell || !shell->write || !shellLogShouldPrint(module, level)) {
        return;
    }
    
    char buffer[512];
    va_list args;
    va_start(args, format);
    
    /* Build log message */
    int offset = 0;
    
    /* Add timestamp if enabled */
    if (g_shell_log_config.timestamp_enabled) {
        uint32_t tick = HAL_GetTick();
        uint32_t seconds = tick / 1000;
        uint32_t milliseconds = tick % 1000;
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%lu.%03lu] ", 
                          seconds, milliseconds);
    }
    
    /* Add colored level and module tag */
    if (g_shell_log_config.color_enabled) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s[%s:%s]%s ", 
                          shellLogGetLevelColor(level),
                          shellLogGetLevelName(level), 
                          shellLogGetModuleName(module),
                          SHELL_COLOR_RESET);
    } else {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s:%s] ", 
                          shellLogGetLevelName(level), shellLogGetModuleName(module));
    }
    
    /* Add user message (without color) */
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, format, args);
    
    /* Ensure newline */
    if (offset > 0 && buffer[offset - 1] != '\n') {
        if (offset < sizeof(buffer) - 2) {
            buffer[offset++] = '\r';
            buffer[offset++] = '\n';
            buffer[offset] = '\0';
        }
    }
    
    /* Print to shell - this is the correct way to maintain shell control */
    shellPrint(shell, "%s", buffer);
    
    va_end(args);
}