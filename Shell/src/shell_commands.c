/**
 * @file shell_commands.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell commands for STM32H725 project
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#include "shell.h"
#include "shell_port.h"
#include "shell_log.h"
#include "main.h"
#include "clock_management.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "fs_manager.h"
#include "audio_recorder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =================================================================== */
/* File System Helper Functions                                       */
/* =================================================================== */

/**
 * @brief 检查文件系统是否可用（使用全局管理器）
 * @retval 0: 可用, -1: 不可用
 */
static int check_filesystem_ready(void)
{
    if (fs_manager_check_status() != 0) {
        SHELL_LOG_FATFS_ERROR("File system not ready");
        SHELL_LOG_FATFS_ERROR("Error: SD card file system is not accessible");
        SHELL_LOG_FATFS_ERROR("Please check if SD card is inserted and formatted");
        return -1;
    }
    return 0;
}

/* 
 * 兼容性宏定义：将原有的f_mount调用重定向到全局文件系统管理器
 * 这样可以最小化代码修改，同时实现统一管理
 */
#define MOUNT_FILESYSTEM() check_filesystem_ready()
#define UNMOUNT_FILESYSTEM() do { /* 不需要卸载，由全局管理器管理 */ } while(0)

/* =================================================================== */
/* Global Current Directory Management                                */
/* =================================================================== */

static char current_working_directory[256] = "/";  // 全局当前工作目录

/* 获取当前工作目录 */
const char* get_current_directory(void)
{
    return current_working_directory;
}

/* 设置当前工作目录 */
void set_current_directory(const char* path)
{
    if (path && strlen(path) < sizeof(current_working_directory)) {
        strcpy(current_working_directory, path);
        
        // 确保路径以'/'结尾（除非是根目录）
        if (strlen(current_working_directory) > 1 && 
            current_working_directory[strlen(current_working_directory) - 1] != '/') {
            strcat(current_working_directory, "/");
        }
        
        // 确保路径以'/'开头
        if (current_working_directory[0] != '/') {
            memmove(current_working_directory + 1, current_working_directory, 
                   strlen(current_working_directory) + 1);
            current_working_directory[0] = '/';
        }
    }
}

/* 标准化路径 - 优化版本，减少栈使用 */
void normalize_path(char* path)
{
    if (!path) return;
    
    // 如果是相对路径，转换为绝对路径
    if (path[0] != '/') {
        char *temp = pvPortMalloc(512);  // 使用堆内存而不是栈
        if (!temp) {
            SHELL_LOG_USER_ERROR("normalize_path: memory allocation failed");
            return;
        }
        snprintf(temp, 512, "%s%s", current_working_directory, path);
        strncpy(path, temp, 511);
        path[511] = '\0';
        vPortFree(temp);
    }
    
    // 简化路径处理 - 就地处理，不使用大量栈空间
    char *src = path;
    char *dst = path;
    
    // 确保以'/'开头
    if (*src != '/') {
        *dst++ = '/';
    }
    
    while (*src) {
        if (*src == '/') {
            // 跳过重复的斜杠
            while (*src == '/') src++;
            if (*src == '\0') break;  // 结尾的斜杠
            
            // 检查是否是特殊目录
            if (*src == '.' && (src[1] == '/' || src[1] == '\0')) {
                // 当前目录，跳过
                src++;
                continue;
            } else if (*src == '.' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
                // 上级目录，回退
                if (dst > path + 1) {  // 不能回退到根目录之前
                    dst--;  // 跳过当前的'/'
                    while (dst > path && *(dst-1) != '/') dst--;  // 找到上一个'/'
                }
                src += 2;
                continue;
            }
            
            // 添加路径分隔符
            if (dst > path && *(dst-1) != '/') {
                *dst++ = '/';
            }
        } else {
            // 普通字符，直接复制
            *dst++ = *src++;
        }
    }
    
    // 确保字符串正确终止
    if (dst == path) {
        *dst++ = '/';  // 空路径变为根目录
    }
    *dst = '\0';
}

/* Helper function for case-insensitive string comparison */
static int my_strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (*s1 + 32) : *s1;
        char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (*s2 + 32) : *s2;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/* 系统信息命令 */
int cmd_sysinfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("System information requested");
    
    SHELL_LOG_SYS_INFO("=== STM32H725 System Information ===");
    SHELL_LOG_SYS_INFO("CPU ID: 0x%08lX", HAL_GetDEVID());
    SHELL_LOG_SYS_INFO("CPU Rev: 0x%08lX", HAL_GetREVID());
    SHELL_LOG_SYS_INFO("UID: %08lX-%08lX-%08lX", 
           HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    SHELL_LOG_SYS_INFO("System Clock: %lu Hz", HAL_RCC_GetSysClockFreq());
    SHELL_LOG_SYS_INFO("HCLK: %lu Hz", HAL_RCC_GetHCLKFreq());
    SHELL_LOG_SYS_INFO("PCLK1: %lu Hz", HAL_RCC_GetPCLK1Freq());
    SHELL_LOG_SYS_INFO("PCLK2: %lu Hz", HAL_RCC_GetPCLK2Freq());
    SHELL_LOG_SYS_INFO("Tick: %lu ms", HAL_GetTick());
    SHELL_LOG_SYS_INFO("HAL Version: %lu", HAL_GetHalVersion());
    
    // 记录系统状态到日志
    SHELL_LOG_SYS_DEBUG("CPU ID: 0x%08lX, Rev: 0x%08lX", HAL_GetDEVID(), HAL_GetREVID());
    SHELL_LOG_SYS_DEBUG("System Clock: %lu Hz, HCLK: %lu Hz", 
                        HAL_RCC_GetSysClockFreq(), HAL_RCC_GetHCLKFreq());
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sysinfo, cmd_sysinfo, show system information);

/* 内存信息命令 */
int cmd_meminfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
    
    SHELL_LOG_MEM_INFO("Memory status requested");
    
    SHELL_LOG_MEM_INFO("=== Memory Information ===");
    SHELL_LOG_MEM_INFO("Free Heap: %u bytes", free_heap);
    SHELL_LOG_MEM_INFO("Min Free Heap: %u bytes", min_free_heap);
    SHELL_LOG_MEM_INFO("Used Heap: %u bytes", configTOTAL_HEAP_SIZE - free_heap);
    SHELL_LOG_MEM_INFO("Total Heap: %u bytes", configTOTAL_HEAP_SIZE);
    
    float usage_percent = ((float)(configTOTAL_HEAP_SIZE - free_heap) / configTOTAL_HEAP_SIZE) * 100;
    SHELL_LOG_MEM_INFO("Memory Usage: %.1f%%", usage_percent);
    
    // 添加内存状态日志
    if (usage_percent > 80.0f) {
        SHELL_LOG_MEM_WARNING("High memory usage: %.1f%% (%u/%u bytes)", 
                             usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    } else if (usage_percent > 60.0f) {
        SHELL_LOG_MEM_INFO("Memory usage: %.1f%% (%u/%u bytes)", 
                          usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    } else {
        SHELL_LOG_MEM_DEBUG("Memory usage: %.1f%% (%u/%u bytes)", 
                           usage_percent, configTOTAL_HEAP_SIZE - free_heap, configTOTAL_HEAP_SIZE);
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 meminfo, cmd_meminfo, show memory information);

/* 任务信息命令 */
int cmd_taskinfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_TASK_INFO("Task information requested");
    
    SHELL_LOG_TASK_INFO("=== FreeRTOS Task Information ===");
    SHELL_LOG_TASK_INFO("Kernel State: %s", 
           (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? "Running" : "Not Running");
    SHELL_LOG_TASK_INFO("Kernel Tick: %lu", xTaskGetTickCount());
    SHELL_LOG_TASK_INFO("Kernel Frequency: %lu Hz", (uint32_t)configTICK_RATE_HZ);
    
    // 记录任务调度器状态
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        SHELL_LOG_TASK_DEBUG("FreeRTOS scheduler is running, tick: %lu", xTaskGetTickCount());
    } else {
        SHELL_LOG_TASK_WARNING("FreeRTOS scheduler is not running");
    }
    
    #if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    char *pcWriteBuffer = pvPortMalloc(1024);
    if (pcWriteBuffer != NULL) {
        vTaskList(pcWriteBuffer);
        SHELL_LOG_TASK_INFO("Task List:\n%s", pcWriteBuffer);
        SHELL_LOG_TASK_DEBUG("Task list generated successfully");
        vPortFree(pcWriteBuffer);
    } else {
        SHELL_LOG_TASK_ERROR("Failed to allocate memory for task list");
    }
    #endif
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 taskinfo, cmd_taskinfo, show task information);

/* 重启命令 */
int cmd_reboot(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("System reboot requested by user");
    SHELL_LOG_SYS_WARNING("System rebooting...");
    
    SHELL_LOG_SYS_INFO("System rebooting in 100ms...");
    vTaskDelay(100);
    NVIC_SystemReset();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 reboot, cmd_reboot, reboot system);


/* LED控制命令（如果有LED的话） */
int cmd_led(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_WARNING("LED command called without parameters");
        SHELL_LOG_USER_INFO("Usage: led <on|off|toggle>");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        SHELL_LOG_USER_INFO("LED turned ON");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else if (strcmp(argv[1], "off") == 0) {
        SHELL_LOG_USER_INFO("LED turned OFF");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    } else if (strcmp(argv[1], "toggle") == 0) {
        SHELL_LOG_USER_INFO("LED toggled");
        // HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    } else {
        SHELL_LOG_USER_ERROR("Invalid LED parameter: %s", argv[1]);
        SHELL_LOG_USER_ERROR("Invalid parameter. Use: on, off, or toggle");
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 led, cmd_led, control LED);

/* 时钟配置测试命令 */
int cmd_clocktest(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_CLK_INFO("Starting clock profile test");
    
    TestAllClockProfiles();
    
    SHELL_LOG_CLK_INFO("Clock profile test completed");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 clocktest, cmd_clocktest, test all clock profiles);

/* 时钟切换命令 */
int cmd_setclock(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: setclock <profile>");
        SHELL_LOG_USER_INFO("Available profiles:");
        SHELL_LOG_USER_INFO("  0 - 32kHz (Ultra Low Power - LSI)");
        SHELL_LOG_USER_INFO("  1 - 24MHz (Low Power - HSI/4)");
        SHELL_LOG_USER_INFO("  2 - 48MHz (Energy Saving)");
        SHELL_LOG_USER_INFO("  3 - 96MHz (Balanced)");
        SHELL_LOG_USER_INFO("  4 - 128MHz (Standard)");
        SHELL_LOG_USER_INFO("  5 - 200MHz (High Efficiency)");
        SHELL_LOG_USER_INFO("  6 - 300MHz (High Performance)");
        SHELL_LOG_USER_INFO("  7 - 400MHz (Ultra High Performance)");
        SHELL_LOG_USER_INFO("  8 - 550MHz (Maximum Performance)");
        SHELL_LOG_USER_INFO("Current System Clock: %lu Hz (%.1f MHz)", 
               HAL_RCC_GetSysClockFreq(), HAL_RCC_GetSysClockFreq() / 1000000.0f);
        return 0;
    }
    
    int profile = atoi(argv[1]);
    if (profile < 0 || profile > 8) {
        SHELL_LOG_CLK_ERROR("Invalid clock profile %d requested", profile);
        SHELL_LOG_CLK_ERROR("Error: Invalid profile %d. Valid range: 0-8", profile);
        return -1;
    }
    
    SHELL_LOG_CLK_INFO("Switching to clock profile %d", profile);
    
    // 记录切换前的时钟频率
    uint32_t old_freq = HAL_RCC_GetSysClockFreq();
    SHELL_LOG_CLK_DEBUG("Current frequency before switch: %lu Hz", old_freq);
    
    // 执行时钟切换
    ClockProfile_t clock_profile = (ClockProfile_t)profile;
    if (SwitchSystemClock(clock_profile) == HAL_OK) {
        // 切换成功，显示新的时钟频率
        uint32_t new_freq = HAL_RCC_GetSysClockFreq();
        SHELL_LOG_CLK_INFO("Clock switch successful: %lu Hz -> %lu Hz", old_freq, new_freq);
        
        SHELL_LOG_CLK_INFO("Clock switch successful!");
        SHELL_LOG_CLK_INFO("Previous: %lu Hz (%.1f MHz)", old_freq, old_freq / 1000000.0f);
        SHELL_LOG_CLK_INFO("Current:  %lu Hz (%.1f MHz)", new_freq, new_freq / 1000000.0f);
        
        // 显示其他时钟域的频率
        SHELL_LOG_CLK_INFO("HCLK:  %lu Hz (%.1f MHz)", HAL_RCC_GetHCLKFreq(), HAL_RCC_GetHCLKFreq() / 1000000.0f);
        SHELL_LOG_CLK_INFO("PCLK1: %lu Hz (%.1f MHz)", HAL_RCC_GetPCLK1Freq(), HAL_RCC_GetPCLK1Freq() / 1000000.0f);
        SHELL_LOG_CLK_INFO("PCLK2: %lu Hz (%.1f MHz)", HAL_RCC_GetPCLK2Freq(), HAL_RCC_GetPCLK2Freq() / 1000000.0f);
    } else {
        SHELL_LOG_CLK_ERROR("Clock switch to profile %d failed", profile);
        SHELL_LOG_CLK_ERROR("Clock switch failed!");
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 setclock, cmd_setclock, switch system clock profile);

/* 版本信息命令 */
int cmd_version(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_SYS_INFO("Version information requested");
    
    SHELL_LOG_SYS_INFO("=== Firmware Version Information ===");
    SHELL_LOG_SYS_INFO("Shell Version: %s", SHELL_VERSION);
    SHELL_LOG_SYS_INFO("HAL Version: %lu", HAL_GetHalVersion());
    SHELL_LOG_SYS_INFO("FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);
    SHELL_LOG_SYS_INFO("Build Date: %s %s", __DATE__, __TIME__);
    SHELL_LOG_SYS_INFO("MCU: STM32H725AEIX");
    
    // 记录版本信息到日志
    SHELL_LOG_SYS_DEBUG("Shell: %s, HAL: %lu, Build: %s %s", 
                        SHELL_VERSION, HAL_GetHalVersion(), __DATE__, __TIME__);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 version, cmd_version, show version information);

/* 十六进制dump命令 */
int cmd_hexdump(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_WARNING("Hexdump command called with insufficient parameters");
        SHELL_LOG_USER_INFO("Usage: hexdump <address> <length>");
        SHELL_LOG_USER_INFO("Example: hexdump 0x08000000 256");
        return -1;
    }
    
    uint32_t addr = strtoul(argv[1], NULL, 0);
    uint32_t len = strtoul(argv[2], NULL, 0);
    
    if (len > 1024) {
        SHELL_LOG_USER_ERROR("Hexdump length too large: %lu bytes (max 1024)", len);
        SHELL_LOG_USER_ERROR("Length too large, max 1024 bytes");
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Hexdump requested: addr=0x%08lX, len=%lu", addr, len);
    SHELL_LOG_USER_INFO("Hex dump from 0x%08lX, length %lu:", addr, len);
    
    uint8_t *ptr = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i += 16) {
        char hex_line[128];
        char ascii_line[20];
        int hex_pos = 0;
        int ascii_pos = 0;
        
        // 格式化地址
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%08lX: ", addr + i);
        
        // 打印十六进制
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%02X ", ptr[i + j]);
        }
        
        // 补齐空格
        for (uint32_t j = len - i; j < 16; j++) {
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "   ");
        }
        
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, " |");
        
        // 打印ASCII
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            char c = ptr[i + j];
            ascii_pos += snprintf(ascii_line + ascii_pos, sizeof(ascii_line) - ascii_pos, 
                                "%c", (c >= 32 && c <= 126) ? c : '.');
        }
        
        ascii_line[ascii_pos] = '\0';
        SHELL_LOG_USER_INFO("%s%s|", hex_line, ascii_line);
    }
    
    SHELL_LOG_USER_DEBUG("Hexdump completed successfully");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 hexdump, cmd_hexdump, hex dump memory);

/* 日志控制命令 */
int cmd_logctl(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: logctl <command> [args...]");
        SHELL_LOG_USER_INFO("Commands:");
        SHELL_LOG_USER_INFO("  status                    - Show current log configuration");
        SHELL_LOG_USER_INFO("  level <global_level>      - Set global log level (0-4)");
        SHELL_LOG_USER_INFO("  module <mod> <level>      - Set module log level");
        SHELL_LOG_USER_INFO("  color <on|off>            - Enable/disable color output");
        SHELL_LOG_USER_INFO("  timestamp <on|off>        - Enable/disable timestamp");
        SHELL_LOG_USER_INFO("  test                      - Test all log levels");
        SHELL_LOG_USER_INFO("");
        SHELL_LOG_USER_INFO("Log Levels: 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=NONE");
        SHELL_LOG_USER_INFO("Modules: 0=SYS, 1=CLK, 2=MEM, 3=TASK, 4=UART, 5=FATFS, 6=USER");
        return 0;
    }
    
    if (strcmp(argv[1], "status") == 0) {
        SHELL_LOG_USER_INFO("=== Log Configuration Status ===");
        SHELL_LOG_USER_INFO("Global Level: %d (%s)", 
                  g_shell_log_config.global_level, 
                  shellLogGetLevelName(g_shell_log_config.global_level));
        SHELL_LOG_USER_INFO("Color Enabled: %s", 
                  g_shell_log_config.color_enabled ? "Yes" : "No");
        SHELL_LOG_USER_INFO("Timestamp Enabled: %s", 
                  g_shell_log_config.timestamp_enabled ? "Yes" : "No");
        SHELL_LOG_USER_INFO("");
        SHELL_LOG_USER_INFO("Module Levels:");
        for (int i = 0; i < SHELL_LOG_MODULE_MAX; i++) {
            SHELL_LOG_USER_INFO("  %s: %d (%s)", 
                      shellLogGetModuleName(i),
                      g_shell_log_config.module_levels[i],
                      shellLogGetLevelName(g_shell_log_config.module_levels[i]));
        }
    }
    else if (strcmp(argv[1], "level") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl level <0-4>");
            return -1;
        }
        int level = atoi(argv[2]);
        if (level < 0 || level > 4) {
            SHELL_LOG_USER_ERROR("Error: Invalid level %d. Valid range: 0-4", level);
            return -1;
        }
        shellLogSetGlobalLevel((ShellLogLevel_t)level);
        SHELL_LOG_USER_INFO("Global log level set to %d (%s)", 
                  level, shellLogGetLevelName((ShellLogLevel_t)level));
    }
    else if (strcmp(argv[1], "module") == 0) {
        if (argc < 4) {
            SHELL_LOG_USER_ERROR("Usage: logctl module <module_id> <level>");
            return -1;
        }
        int module = atoi(argv[2]);
        int level = atoi(argv[3]);
        if (module < 0 || module >= SHELL_LOG_MODULE_MAX) {
            SHELL_LOG_USER_ERROR("Error: Invalid module %d. Valid range: 0-%d", 
                      module, SHELL_LOG_MODULE_MAX - 1);
            return -1;
        }
        if (level < 0 || level > 4) {
            SHELL_LOG_USER_ERROR("Error: Invalid level %d. Valid range: 0-4", level);
            return -1;
        }
        shellLogSetModuleLevel((ShellLogModule_t)module, (ShellLogLevel_t)level);
        SHELL_LOG_USER_INFO("Module %s log level set to %d (%s)", 
                  shellLogGetModuleName((ShellLogModule_t)module),
                  level, shellLogGetLevelName((ShellLogLevel_t)level));
    }
    else if (strcmp(argv[1], "color") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl color <on|off>");
            return -1;
        }
        uint8_t enable = (strcmp(argv[2], "on") == 0) ? 1 : 0;
        shellLogSetColorEnabled(enable);
        SHELL_LOG_USER_INFO("Color output %s", enable ? "enabled" : "disabled");
    }
    else if (strcmp(argv[1], "timestamp") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: logctl timestamp <on|off>");
            return -1;
        }
        uint8_t enable = (strcmp(argv[2], "on") == 0) ? 1 : 0;
        shellLogSetTimestampEnabled(enable);
        SHELL_LOG_USER_INFO("Timestamp output %s", enable ? "enabled" : "disabled");
    }
    else if (strcmp(argv[1], "test") == 0) {
        SHELL_LOG_USER_INFO("Testing all log levels for all modules:");
        for (int module = 0; module < SHELL_LOG_MODULE_MAX; module++) {
            SHELL_LOG_DEBUG((ShellLogModule_t)module, "Debug message from %s module", 
                           shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_INFO((ShellLogModule_t)module, "Info message from %s module", 
                          shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_WARNING((ShellLogModule_t)module, "Warning message from %s module", 
                             shellLogGetModuleName((ShellLogModule_t)module));
            SHELL_LOG_ERROR((ShellLogModule_t)module, "Error message from %s module", 
                           shellLogGetModuleName((ShellLogModule_t)module));
        }
        SHELL_LOG_USER_INFO("Log test completed");
    }
    else {
        SHELL_LOG_USER_ERROR("Unknown command: %s", argv[1]);
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 logctl, cmd_logctl, log control and configuration);

/* 日志测试命令 */
int cmd_logtest(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_INFO("Usage: logtest <module_id>");
        SHELL_LOG_USER_INFO("Modules: 0=SYS, 1=CLK, 2=MEM, 3=TASK, 4=UART, 5=FATFS, 6=USER");
        return -1;
    }
    
    int module = atoi(argv[1]);
    if (module < 0 || module >= SHELL_LOG_MODULE_MAX) {
        SHELL_LOG_USER_ERROR("Error: Invalid module %d. Valid range: 0-%d", 
                  module, SHELL_LOG_MODULE_MAX - 1);
        return -1;
    }
    
    SHELL_LOG_USER_INFO("Testing log output for module %s:", 
              shellLogGetModuleName((ShellLogModule_t)module));
    
    SHELL_LOG_DEBUG((ShellLogModule_t)module, "This is a DEBUG message");
    SHELL_LOG_INFO((ShellLogModule_t)module, "This is an INFO message");
    SHELL_LOG_WARNING((ShellLogModule_t)module, "This is a WARNING message");
    SHELL_LOG_ERROR((ShellLogModule_t)module, "This is an ERROR message");
    
    SHELL_LOG_USER_INFO("Log test completed for module %s", 
              shellLogGetModuleName((ShellLogModule_t)module));
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 logtest, cmd_logtest, test log output for specific module);

/* 变量导出示例 */
static int test_var = 12345;
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_INT), 
                 testVar, &test_var, test variable);

static char test_string[] = "Hello STM32H725!";
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_STRING), 
                 testStr, test_string, test string variable);

/* SD卡文件写入命令 */
int cmd_sdwrite(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_WARNING("SD write command called with insufficient parameters");
        SHELL_LOG_USER_INFO("Usage: sdwrite <filename> <content>");
        SHELL_LOG_USER_INFO("Example: sdwrite test.txt \"Hello World!\"");
        SHELL_LOG_USER_INFO("Note: Filename should end with .txt extension");
        return -1;
    }
    
    char *filename = argv[1];
    char *content = argv[2];
    
    // 验证文件名
    if (strstr(filename, ".txt") == NULL) {
        SHELL_LOG_USER_WARNING("File name should have .txt extension: %s", filename);
        SHELL_LOG_USER_WARNING("Warning: Recommended to use .txt extension for text files");
    }
    
    SHELL_LOG_FATFS_INFO("Starting SD card file write operation");
    SHELL_LOG_FATFS_INFO("Target file: %s, Content length: %d", filename, strlen(content));
    
    // 检查文件系统是否可用
    if (check_filesystem_ready() != 0) {
        return -1;
    }
    
    SHELL_LOG_FATFS_DEBUG("SD card filesystem is ready");
    
    // 打开文件进行写入（如果不存在则创建）
    FRESULT fr = f_open(&USERFile, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to create/open file %s: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: Cannot create file '%s' (Error: %d)", filename, fr);
        return -1;
    }
    
    SHELL_LOG_FATFS_DEBUG("File %s opened successfully for writing", filename);
    
    // 写入内容
    UINT bytes_written;
    fr = f_write(&USERFile, content, strlen(content), &bytes_written);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to write to file %s: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: Write operation failed (Error: %d)", fr);
        f_close(&USERFile);
        return -1;
    }
    
    // 检查写入的字节数
    if (bytes_written != strlen(content)) {
        SHELL_LOG_FATFS_WARNING("Partial write: expected %d bytes, wrote %d bytes", 
                               strlen(content), bytes_written);
        SHELL_LOG_FATFS_WARNING("Warning: Partial write detected");
    }
    
    SHELL_LOG_FATFS_DEBUG("Successfully wrote %d bytes to file", bytes_written);
    
    // 同步并关闭文件
    fr = f_sync(&USERFile);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_WARNING("Failed to sync file %s: %d", filename, fr);
        SHELL_LOG_FATFS_WARNING("Warning: File sync failed, data may not be saved");
    }
    
    fr = f_close(&USERFile);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_WARNING("Failed to close file %s: %d", filename, fr);
        SHELL_LOG_FATFS_WARNING("Warning: File close failed");
    }
    
    SHELL_LOG_FATFS_INFO("File operation completed successfully");
    SHELL_LOG_FATFS_INFO("Success: File '%s' created with %d bytes", filename, bytes_written);
    SHELL_LOG_FATFS_INFO("Content: \"%s\"", content);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sdwrite, cmd_sdwrite, write text content to SD card file);

/* SD卡文件读取命令 */
int cmd_sdread(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_WARNING("SD read command called without filename");
        SHELL_LOG_USER_INFO("Usage: sdread <filename>");
        SHELL_LOG_USER_INFO("Example: sdread test.txt");
        return -1;
    }
    
    char *filename = argv[1];
    
    SHELL_LOG_FATFS_INFO("Starting SD card file read operation");
    SHELL_LOG_FATFS_INFO("Target file: %s", filename);

    // 检查文件系统是否可用
    if (check_filesystem_ready() != 0) {
        return -1;
    }
    
    SHELL_LOG_FATFS_DEBUG("SD card filesystem is ready");
    
    // 打开文件进行读取
    FRESULT fr = f_open(&USERFile, filename, FA_READ);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to open file %s: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: Cannot open file '%s' (Error: %d)", filename, fr);
        return -1;
    }
    
    SHELL_LOG_FATFS_DEBUG("File %s opened successfully for reading", filename);
    
    // 获取文件大小
    FSIZE_t file_size = f_size(&USERFile);
    if (file_size > 1024) {
        SHELL_LOG_FATFS_WARNING("File size %lu bytes is large, truncating to 1024 bytes", file_size);
        SHELL_LOG_FATFS_WARNING("Warning: File is large (%lu bytes), showing first 1024 bytes only", file_size);
        file_size = 1024;
    }
    
    // 分配读取缓冲区
    char *buffer = pvPortMalloc(file_size + 1);
    if (buffer == NULL) {
        SHELL_LOG_FATFS_ERROR("Failed to allocate %lu bytes for file buffer", file_size + 1);
        SHELL_LOG_FATFS_ERROR("Error: Memory allocation failed");
        f_close(&USERFile);
        return -1;
    }
    
    // 读取文件内容
    UINT bytes_read;
    fr = f_read(&USERFile, buffer, file_size, &bytes_read);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to read file %s: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: Read operation failed (Error: %d)", fr);
        vPortFree(buffer);
        f_close(&USERFile);
        return -1;
    }
    
    buffer[bytes_read] = '\0';  // 添加字符串结束符
    
    // 关闭文件
    f_close(&USERFile);
    
    SHELL_LOG_FATFS_INFO("File read completed successfully");
    SHELL_LOG_FATFS_INFO("=== File Content (%d bytes) ===", bytes_read);
    SHELL_LOG_FATFS_INFO("File: %s", filename);
    SHELL_LOG_FATFS_INFO("Size: %d bytes", bytes_read);
    SHELL_LOG_FATFS_INFO("Content:");
    SHELL_LOG_FATFS_INFO("%s", buffer);
    SHELL_LOG_FATFS_INFO("=== End of File ===");
    
    vPortFree(buffer);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sdread, cmd_sdread, read text content from SD card file);

/* SD卡目录列表命令 */
int cmd_sdls(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;

    char *path = "/";  // 默认根目录
    if (argc >= 2) {
        path = argv[1];
    }
    
    SHELL_LOG_FATFS_INFO("Listing SD card directory: %s", path);
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    DIR dir;
    FILINFO fno;
    
    // 打开目录
    FRESULT fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to open directory %s: %d", path, fr);
        SHELL_LOG_FATFS_ERROR("Error: Cannot open directory '%s' (Error: %d)", path, fr);
        
        return -1;
    }
    
    SHELL_LOG_FATFS_INFO("=== Directory Listing: %s ===", path);
    
    int file_count = 0;
    int dir_count = 0;
    FSIZE_t total_size = 0;
    
    // 读取目录内容
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;  // 错误或到达目录末尾
        
        if (fno.fattrib & AM_DIR) {
            // 目录
            SHELL_LOG_FATFS_INFO("<DIR>     %s", fno.fname);
            dir_count++;
        } else {
            // 文件
            SHELL_LOG_FATFS_INFO("%8lu  %s", fno.fsize, fno.fname);
            file_count++;
            total_size += fno.fsize;
        }
    }
    
    SHELL_LOG_FATFS_INFO("=== Summary ===");
    SHELL_LOG_FATFS_INFO("Directories: %d", dir_count);
    SHELL_LOG_FATFS_INFO("Files: %d", file_count);
    SHELL_LOG_FATFS_INFO("Total size: %lu bytes", total_size);
    
    // 获取磁盘空间信息
    FATFS *fs;
    DWORD fre_clust;
    fr = f_getfree(USERPath, &fre_clust, &fs);
    if (fr == FR_OK) {
        DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
        DWORD fre_sect = fre_clust * fs->csize;
        SHELL_LOG_FATFS_INFO("Total space: %lu KB", tot_sect / 2);
        SHELL_LOG_FATFS_INFO("Free space: %lu KB", fre_sect / 2);
    }
    
    f_closedir(&dir);
    
    
    SHELL_LOG_FATFS_DEBUG("Directory listing completed successfully");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sdls, cmd_sdls, list SD card directory contents);

/* SD卡文件删除命令 */
int cmd_sdrm(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_WARNING("SD remove command called without filename");
        SHELL_LOG_USER_INFO("Usage: sdrm <filename>");
        SHELL_LOG_USER_INFO("Example: sdrm test.txt");
        SHELL_LOG_USER_INFO("Warning: This will permanently delete the file!");
        return -1;
    }
    
    char *filename = argv[1];
    
    SHELL_LOG_FATFS_WARNING("File deletion requested: %s", filename);
    SHELL_LOG_FATFS_WARNING("Attempting to delete file: %s", filename);
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    // 检查文件是否存在
    FILINFO fno;
    FRESULT fr = f_stat(filename, &fno);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("File %s not found or inaccessible: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: File '%s' not found (Error: %d)", filename, fr);
        
        return -1;
    }
    
    SHELL_LOG_FATFS_DEBUG("File %s found, size: %lu bytes", filename, fno.fsize);
    
    // 删除文件
    fr = f_unlink(filename);
    if (fr != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Failed to delete file %s: %d", filename, fr);
        SHELL_LOG_FATFS_ERROR("Error: Cannot delete file '%s' (Error: %d)", filename, fr);
        
        return -1;
    }
    
    
    
    SHELL_LOG_FATFS_INFO("File deletion completed successfully");
    SHELL_LOG_FATFS_INFO("Success: File '%s' deleted", filename);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sdrm, cmd_sdrm, delete file from SD card);

/* =================================================================== */
/* Linux-like File System Commands for SD Card                       */
/* =================================================================== */

/* ls命令 - 列出目录内容 */
int cmd_ls(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    char *input_path = ".";  // 默认当前目录
    uint8_t show_all = 0;   // -a选项
    uint8_t long_format = 0; // -l选项
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'a') show_all = 1;
                else if (argv[i][j] == 'l') long_format = 1;
                else {
                    SHELL_LOG_USER_ERROR("Unknown option: -%c", argv[i][j]);
                    SHELL_LOG_USER_INFO("Usage: ls [-al] [directory]");
                    return -1;
                }
            }
        } else {
            input_path = argv[i];
        }
    }
    
    // 使用堆内存构建完整路径，避免栈溢出
    char *full_path = pvPortMalloc(512);
    if (!full_path) {
        SHELL_LOG_USER_ERROR("ls: memory allocation failed");
        return -1;
    }
    
    if (input_path[0] == '/') {
        // 绝对路径
        strncpy(full_path, input_path, 511);
        full_path[511] = '\0';
    } else if (strcmp(input_path, ".") == 0) {
        // 当前目录
        strncpy(full_path, get_current_directory(), 511);
        full_path[511] = '\0';
    } else {
        // 相对路径
        snprintf(full_path, 512, "%s%s", get_current_directory(), input_path);
    }

    // 标准化路径
    normalize_path(full_path);

    SHELL_LOG_FATFS_DEBUG("ls command: path=%s, show_all=%d, long_format=%d", full_path, show_all, long_format);

    // 安全地挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }

    DIR dir;
    FILINFO fno;
    
    // 清零文件信息结构
    memset(&fno, 0, sizeof(fno));
    
    // 打开目录
    FRESULT fr = f_opendir(&dir, full_path);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("ls: cannot access '%s': No such directory (error %d)", full_path, fr);
        
        vPortFree(full_path);
        return -1;
    }
    
    int file_count = 0;
    int dir_count = 0;
    FSIZE_t total_size = 0;
    int max_entries = 1000;  // 限制最大条目数，防止无限循环
    
    // 读取目录内容
    for (int entry_count = 0; entry_count < max_entries; entry_count++) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        
        // 每10个条目喂一次看门狗
        if (entry_count % 10 == 0) {
            taskYIELD();  // 让其他任务有机会运行
        }
        
        // 跳过隐藏文件（除非使用-a选项）
        if (!show_all && fno.fname[0] == '.') continue;
        
        // 修复乱码问题：清理文件名中的非打印字符
        for (int i = 0; fno.fname[i]; i++) {
            if (fno.fname[i] < ' ' || fno.fname[i] > '~') {
                fno.fname[i] = '?';
            }
        }

        char *name_to_print = fno.fname;
        char name_buffer[300];
#if defined(_USE_LFN) && (_USE_LFN > 0)
        if (fno.altname[0]) {
             snprintf(name_buffer, sizeof(name_buffer), "%-13s %s", fno.altname, fno.fname);
             name_to_print = name_buffer;
        }
#endif
        
        if (fno.fattrib & AM_DIR) {
            if (long_format) {
                SHELL_LOG_USER_INFO("drwxr-xr-x 1 root root %8s %s", "4096", name_to_print);
            } else {
                SHELL_LOG_USER_INFO("%s/", name_to_print);
            }
            dir_count++;
        } else {
            if (long_format) {
                char permissions[] = "-rw-r--r--";
                if (fno.fattrib & AM_RDO) permissions[2] = '-';
                // FSIZE_t在exFAT启用时是64位，强转为unsigned long以匹配%lu格式，避免64位/32位参数错位导致的非法内存访问
                SHELL_LOG_USER_INFO("%s 1 root root %8lu %s", permissions, (unsigned long)fno.fsize, name_to_print);
            } else {
                SHELL_LOG_USER_INFO("%s", name_to_print);
            }
            file_count++;
            total_size += fno.fsize;
        }
        
        // 如果条目过多，显示警告并退出
        if (entry_count >= max_entries - 1) {
            SHELL_LOG_USER_INFO("... (too many entries, truncated)");
            break;
        }
    }
    
    if (long_format) {
        SHELL_LOG_USER_INFO("total %d", file_count + dir_count);
    }
    
    f_closedir(&dir);

    vPortFree(full_path);  // 释放分配的内存
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 ls, cmd_ls, list directory contents);

/* mkdir命令 - 创建目录 */
int cmd_mkdir(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    FRESULT fr;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("mkdir: missing operand");
        SHELL_LOG_USER_INFO("Usage: mkdir [-p] directory...");
        return -1;
    }
    
    uint8_t create_parents = 0;
    int start_idx = 1;
    
    // 解析-p选项
    if (argc > 2 && strcmp(argv[1], "-p") == 0) {
        create_parents = 1;
        start_idx = 2;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    int success_count = 0;
    
    for (int i = start_idx; i < argc; i++) {
        char *dirname = argv[i];
        
        if (create_parents) {
            // 创建父目录（简化实现）
            char path_copy[256];
            strncpy(path_copy, dirname, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';
            
            char *token = strtok(path_copy, "/");
            char current_path[256] = "";
            
            while (token != NULL) {
                if (strlen(current_path) > 0) {
                    strcat(current_path, "/");
                }
                strcat(current_path, token);
                
                fr = f_mkdir(current_path);
                if (fr != FR_OK && fr != FR_EXIST) {
                    SHELL_LOG_USER_ERROR("mkdir: cannot create directory '%s': %d", current_path, fr);
                    break;
                }
                
                token = strtok(NULL, "/");
            }
        } else {
            fr = f_mkdir(dirname);
            if (fr != FR_OK) {
                if (fr == FR_EXIST) {
                    SHELL_LOG_USER_ERROR("mkdir: cannot create directory '%s': File exists", dirname);
                } else {
                    SHELL_LOG_USER_ERROR("mkdir: cannot create directory '%s': %d", dirname, fr);
                }
                continue;
            }
        }
        
        SHELL_LOG_FATFS_INFO("Directory created: %s", dirname);
        success_count++;
    }
    
    
    
    return (success_count > 0) ? 0 : -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 mkdir, cmd_mkdir, create directories);

/* rm命令 - 删除文件和目录 */
int cmd_rm(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("rm: missing operand");
        SHELL_LOG_USER_INFO("Usage: rm [-rf] file...");
        return -1;
    }
    
    uint8_t recursive = 0;
    uint8_t force = 0;
    int start_idx = 1;
    
    // 解析选项
    if (argc > 2 && argv[1][0] == '-') {
        for (int j = 1; argv[1][j] != '\0'; j++) {
            if (argv[1][j] == 'r') recursive = 1;
            else if (argv[1][j] == 'f') force = 1;
            else {
                SHELL_LOG_USER_ERROR("rm: invalid option -- '%c'", argv[1][j]);
                return -1;
            }
        }
        start_idx = 2;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    int success_count = 0;
    
    for (int i = start_idx; i < argc; i++) {
        char *target = argv[i];
        
        FILINFO fno;
        FRESULT fr = f_stat(target, &fno);
        if (fr != FR_OK) {
            if (!force) {
                SHELL_LOG_USER_ERROR("rm: cannot remove '%s': No such file or directory", target);
            }
            continue;
        }
        
        if (fno.fattrib & AM_DIR) {
            if (!recursive) {
                SHELL_LOG_USER_ERROR("rm: cannot remove '%s': Is a directory", target);
                continue;
            }
            // 删除目录（简化实现，只删除空目录）
            fr = f_unlink(target);
        } else {
            // 删除文件
            fr = f_unlink(target);
        }
        
        if (fr != FR_OK) {
            if (!force) {
                SHELL_LOG_USER_ERROR("rm: cannot remove '%s': %d", target, fr);
            }
        } else {
            SHELL_LOG_FATFS_INFO("Removed: %s", target);
            success_count++;
        }
    }
    
    
    
    return (success_count > 0) ? 0 : -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 rm, cmd_rm, remove files and directories);

/* touch命令 - 创建空文件或更新时间戳 */
int cmd_touch(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("touch: missing file operand");
        SHELL_LOG_USER_INFO("Usage: touch file...");
        return -1;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    int success_count = 0;
    
    for (int i = 1; i < argc; i++) {
        char *filename = argv[i];
        
        // 尝试打开文件，如果不存在则创建
        FRESULT fr = f_open(&USERFile, filename, FA_OPEN_EXISTING | FA_WRITE);
        if (fr == FR_NO_FILE) {
            // 文件不存在，创建新文件
            FRESULT fr = f_open(&USERFile, filename, FA_CREATE_NEW | FA_WRITE);
            if (fr == FR_OK) {
                SHELL_LOG_FATFS_INFO("Created: %s", filename);
                f_close(&USERFile);
                success_count++;
            } else {
                SHELL_LOG_USER_ERROR("touch: cannot touch '%s': %d", filename, fr);
            }
        } else if (fr == FR_OK) {
            // 文件存在，更新时间戳（通过写入来实现）
            f_sync(&USERFile);
            f_close(&USERFile);
            SHELL_LOG_FATFS_INFO("Updated: %s", filename);
            success_count++;
        } else {
            SHELL_LOG_USER_ERROR("touch: cannot touch '%s': %d", filename, fr);
        }
    }
    
    
    
    return (success_count > 0) ? 0 : -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 touch, cmd_touch, create empty files or update timestamps);

/* cp命令 - 复制文件 */
int cmd_cp(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_ERROR("cp: missing destination file operand");
        SHELL_LOG_USER_INFO("Usage: cp [-r] source dest");
        return -1;
    }
    
    uint8_t recursive = 0;
    int src_idx = 1;
    int dst_idx = 2;
    
    // 解析-r选项
    if (argc > 3 && strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        src_idx = 2;
        dst_idx = 3;
    }
    
    char *source = argv[src_idx];
    char *dest = argv[dst_idx];
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    // 检查源文件
    FILINFO src_info;
    FRESULT fr = f_stat(source, &src_info);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("cp: cannot stat '%s': No such file or directory", source);
        
        return -1;
    }
    
    if (src_info.fattrib & AM_DIR) {
        if (!recursive) {
            SHELL_LOG_USER_ERROR("cp: -r not specified; omitting directory '%s'", source);
            
            return -1;
        }
        SHELL_LOG_USER_ERROR("cp: directory copying not fully implemented");
        
        return -1;
    }
    
    // 打开源文件
    FIL src_file;
    fr = f_open(&src_file, source, FA_READ);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("cp: cannot open '%s' for reading: %d", source, fr);
        
        return -1;
    }
    
    // 创建目标文件
    FIL dst_file;
    fr = f_open(&dst_file, dest, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("cp: cannot create '%s': %d", dest, fr);
        f_close(&src_file);
        
        return -1;
    }
    
    // 复制文件内容
    char buffer[512];
    UINT bytes_read, bytes_written;
    FSIZE_t total_copied = 0;
    
    while (1) {
        fr = f_read(&src_file, buffer, sizeof(buffer), &bytes_read);
        if (fr != FR_OK || bytes_read == 0) break;
        
        fr = f_write(&dst_file, buffer, bytes_read, &bytes_written);
        if (fr != FR_OK || bytes_written != bytes_read) {
            SHELL_LOG_USER_ERROR("cp: write error: %d", fr);
            break;
        }
        
        total_copied += bytes_written;
    }
    
    f_close(&src_file);
    f_close(&dst_file);
    
    
    if (fr == FR_OK) {
        SHELL_LOG_FATFS_INFO("Copied %lu bytes from '%s' to '%s'", total_copied, source, dest);
        return 0;
    } else {
        SHELL_LOG_USER_ERROR("cp: copy failed: %d", fr);
        return -1;
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 cp, cmd_cp, copy files and directories);

/* mv命令 - 移动/重命名文件 */
int cmd_mv(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_ERROR("mv: missing destination file operand");
        SHELL_LOG_USER_INFO("Usage: mv source dest");
        return -1;
    }
    
    char *source = argv[1];
    char *dest = argv[2];
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    // 检查源文件是否存在
    FILINFO src_info;
    FRESULT fr = f_stat(source, &src_info);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("mv: cannot stat '%s': No such file or directory", source);
        
        return -1;
    }
    
    // 尝试重命名
    fr = f_rename(source, dest);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("mv: cannot move '%s' to '%s': %d", source, dest, fr);
        
        return -1;
    }
    
    
    SHELL_LOG_FATFS_INFO("Moved '%s' to '%s'", source, dest);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 mv, cmd_mv, move/rename files and directories);

/* cat命令 - 显示文件内容 */
int cmd_cat(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("cat: missing file operand");
        SHELL_LOG_USER_INFO("Usage: cat file...");
        return -1;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    int success_count = 0;
    
    for (int i = 1; i < argc; i++) {
        char *filename = argv[i];
        
        FRESULT fr = f_open(&USERFile, filename, FA_READ);
        if (fr != FR_OK) {
            SHELL_LOG_USER_ERROR("cat: %s: No such file or directory", filename);
            continue;
        }
        
        // 读取并显示文件内容
        char buffer[256];
        UINT bytes_read;
        
        while (1) {
            fr = f_read(&USERFile, buffer, sizeof(buffer) - 1, &bytes_read);
            if (fr != FR_OK || bytes_read == 0) break;
            
            buffer[bytes_read] = '\0';
            shellWriteString(shell, buffer);
        }
        
        f_close(&USERFile);
        success_count++;
        
        // 在每个文件输出后添加换行符，确保格式正确
        shellWriteString(shell, "\r\n");
    }
    
    
    
    return (success_count > 0) ? 0 : -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 cat, cmd_cat, display file contents);

/* pwd命令 - 显示当前工作目录 */
int cmd_pwd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    // 直接输出全局当前工作目录
    const char* cwd = get_current_directory();
    SHELL_LOG_USER_INFO("%s", cwd);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 pwd, cmd_pwd, print working directory);

/* cd命令 - 切换目录 */
int cmd_cd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;

    char *target_dir = "/";  // 默认根目录
    if (argc >= 2) {
        target_dir = argv[1];
    }
    
    // 处理特殊目录
    if (strcmp(target_dir, "~") == 0 || strcmp(target_dir, "") == 0) {
        target_dir = "/";
    }
    
    // 构建完整路径
    char full_path[512];
    if (target_dir[0] == '/') {
        // 绝对路径
        strcpy(full_path, target_dir);
    } else {
        // 相对路径，基于当前目录
        snprintf(full_path, sizeof(full_path), "%s%s", get_current_directory(), target_dir);
    }

    // 标准化路径
    normalize_path(full_path);

    // 挂载文件系统验证目录是否存在
    if (check_filesystem_ready() != 0) { return -1; }

    // 检查目录是否存在
    FILINFO fno;
    FRESULT fr = f_stat(full_path, &fno);
    if (fr != FR_OK) {
        if (strcmp(full_path, "/") != 0) {  // 根目录总是存在
            SHELL_LOG_USER_ERROR("cd: %s: No such file or directory", target_dir);
            
            return -1;
        }
    } else if (!(fno.fattrib & AM_DIR)) {
        SHELL_LOG_USER_ERROR("cd: %s: Not a directory", target_dir);
        
        return -1;
    }
    
    // 更新全局当前目录
    set_current_directory(full_path);
    
    
    SHELL_LOG_FATFS_DEBUG("Changed directory to: %s", get_current_directory());
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 cd, cmd_cd, change directory);

/* df命令 - 显示磁盘空间使用情况 */
int cmd_df(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    FRESULT fr;
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    FATFS *fs;
    DWORD fre_clust;
    fr = f_getfree(USERPath, &fre_clust, &fs);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("df: cannot get filesystem information: %d", fr);
        
        return -1;
    }
    
    DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
    DWORD fre_sect = fre_clust * fs->csize;
    DWORD used_sect = tot_sect - fre_sect;
    
    SHELL_LOG_USER_INFO("Filesystem     1K-blocks     Used Available Use%% Mounted on");
    SHELL_LOG_USER_INFO("%-14s %9lu %8lu %9lu %3d%% %s", 
              "/dev/sdcard", 
              tot_sect / 2,     // Total in KB
              used_sect / 2,    // Used in KB  
              fre_sect / 2,     // Available in KB
              (int)((used_sect * 100) / tot_sect),  // Use percentage
              "/");
    
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 df, cmd_df, display filesystem disk space usage);

/* find命令 - 查找文件 */
int cmd_find(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_ERROR("find: missing arguments");
        SHELL_LOG_USER_INFO("Usage: find path -name pattern");
        return -1;
    }
    
    char *search_path = argv[1];
    char *pattern = NULL;
    
    // 简单解析-name选项
    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-name") == 0) {
            pattern = argv[i + 1];
            break;
        }
    }
    
    if (!pattern) {
        SHELL_LOG_USER_ERROR("find: -name option required");
        return -1;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    DIR dir;
    FILINFO fno;
    
    FRESULT fr = f_opendir(&dir, search_path);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("find: '%s': No such directory", search_path);
        
        return -1;
    }
    
    int found_count = 0;
    
    // 简单的文件名匹配（支持*通配符）
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        // 简单的通配符匹配
        if (strcmp(pattern, "*") == 0 || strstr(fno.fname, pattern) != NULL) {
            char full_path[512];  // 增加缓冲区大小以避免截断
            if (strcmp(search_path, "/") == 0) {
                snprintf(full_path, sizeof(full_path), "/%s", fno.fname);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", search_path, fno.fname);
            }
            SHELL_LOG_USER_INFO("%s", full_path);
            found_count++;
        }
    }
    
    f_closedir(&dir);
    
    
    SHELL_LOG_FATFS_DEBUG("Found %d files matching pattern '%s'", found_count, pattern);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 find, cmd_find, search for files and directories);

/* wc命令 - 统计文件行数、字数、字符数 */
int cmd_wc(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("wc: missing file operand");
        SHELL_LOG_USER_INFO("Usage: wc [-lwc] file...");
        return -1;
    }
    
    uint8_t count_lines = 1;
    uint8_t count_words = 1;
    uint8_t count_chars = 1;
    int start_idx = 1;
    
    // 解析选项
    if (argc > 2 && argv[1][0] == '-') {
        count_lines = count_words = count_chars = 0;
        for (int j = 1; argv[1][j] != '\0'; j++) {
            if (argv[1][j] == 'l') count_lines = 1;
            else if (argv[1][j] == 'w') count_words = 1;
            else if (argv[1][j] == 'c') count_chars = 1;
        }
        start_idx = 2;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    for (int i = start_idx; i < argc; i++) {
        char *filename = argv[i];
        
        FRESULT fr = f_open(&USERFile, filename, FA_READ);
        if (fr != FR_OK) {
            SHELL_LOG_USER_ERROR("wc: %s: No such file", filename);
            continue;
        }
        
        UINT lines = 0, words = 0, chars = 0;
        char buffer[256];
        UINT bytes_read;
        uint8_t in_word = 0;
        
        while (1) {
            fr = f_read(&USERFile, buffer, sizeof(buffer), &bytes_read);
            if (fr != FR_OK || bytes_read == 0) break;
            
            for (UINT j = 0; j < bytes_read; j++) {
                chars++;
                
                if (buffer[j] == '\n') {
                    lines++;
                }
                
                if (buffer[j] == ' ' || buffer[j] == '\t' || buffer[j] == '\n') {
                    in_word = 0;
                } else if (!in_word) {
                    words++;
                    in_word = 1;
                }
            }
        }
        
        f_close(&USERFile);
        
        // 输出统计结果
        char result[128] = "";
        if (count_lines) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%8u ", lines);
            strcat(result, temp);
        }
        if (count_words) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%8u ", words);
            strcat(result, temp);
        }
        if (count_chars) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%8u ", chars);
            strcat(result, temp);
        }
        
        SHELL_LOG_USER_INFO("%s%s", result, filename);
    }
    
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 wc, cmd_wc, print newline word and byte counts);

/* head命令 - 显示文件开头几行 */
int cmd_head(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;

    int num_lines = 10;  // 默认10行
    int file_idx = 1;

    // 解析-n选项
    if (argc > 3 && strcmp(argv[1], "-n") == 0) {
        num_lines = atoi(argv[2]);
        file_idx = 3;
    } else if (argc > 2 && argv[1][0] == '-' && argv[1][1] != '\0') {
        num_lines = atoi(&argv[1][1]);
        file_idx = 2;
    }
    
    if (file_idx >= argc) {
        SHELL_LOG_USER_ERROR("head: missing file operand");
        SHELL_LOG_USER_INFO("Usage: head [-n NUM] file");
        return -1;
    }
    
    char *filename = argv[file_idx];
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    FRESULT fr = f_open(&USERFile, filename, FA_READ);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("head: %s: No such file", filename);
        
        return -1;
    }
    
    char buffer[256];
    UINT bytes_read;
    int lines_printed = 0;
    
    while (lines_printed < num_lines) {
        fr = f_read(&USERFile, buffer, sizeof(buffer) - 1, &bytes_read);
        if (fr != FR_OK || bytes_read == 0) break;
        
        buffer[bytes_read] = '\0';
        
        for (UINT i = 0; i < bytes_read && lines_printed < num_lines; i++) {
            char temp_str[2] = {buffer[i], '\0'};
            shellWriteString(shell, temp_str);
            if (buffer[i] == '\n') {
                lines_printed++;
            }
        }
    }
    
    f_close(&USERFile);
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 head, cmd_head, output the first part of files);

/* tail命令 - 显示文件末尾几行 */
int cmd_tail(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;

    int num_lines = 10;  // 默认10行
    int file_idx = 1;

    // 解析-n选项
    if (argc > 3 && strcmp(argv[1], "-n") == 0) {
        num_lines = atoi(argv[2]);
        file_idx = 3;
    } else if (argc > 2 && argv[1][0] == '-' && argv[1][1] != '\0') {
        num_lines = atoi(&argv[1][1]);
        file_idx = 2;
    }
    
    if (file_idx >= argc) {
        SHELL_LOG_USER_ERROR("tail: missing file operand");
        SHELL_LOG_USER_INFO("Usage: tail [-n NUM] file");
        return -1;
    }
    
    char *filename = argv[file_idx];
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    FRESULT fr = f_open(&USERFile, filename, FA_READ);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("tail: %s: No such file", filename);
        
        return -1;
    }
    
    // 简化实现：读取整个文件，然后输出最后几行
    FSIZE_t file_size = f_size(&USERFile);
    if (file_size > 4096) {
        SHELL_LOG_USER_WARNING("tail: file too large, showing approximate tail");
        f_lseek(&USERFile, file_size - 4096);
    }
    
    char *buffer = pvPortMalloc(4096);
    if (!buffer) {
        SHELL_LOG_USER_ERROR("tail: memory allocation failed");
        f_close(&USERFile);
        
        return -1;
    }
    
    UINT bytes_read;
    fr = f_read(&USERFile, buffer, 4096, &bytes_read);
    
    if (fr == FR_OK && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        // 从末尾往前数行数
        int line_count = 0;
        int start_pos = bytes_read - 1;
        
        for (int i = bytes_read - 1; i >= 0; i--) {
            if (buffer[i] == '\n') {
                line_count++;
                if (line_count == num_lines) {
                    start_pos = i + 1;
                    break;
                }
            }
        }
        
        if (line_count < num_lines) start_pos = 0;
        
        shellWriteString(shell, &buffer[start_pos]);
    }
    
    vPortFree(buffer);
    f_close(&USERFile);
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 tail, cmd_tail, output the last part of files);

/* grep命令 - 文本搜索 */
int cmd_grep(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 3) {
        SHELL_LOG_USER_ERROR("grep: missing arguments");
        SHELL_LOG_USER_INFO("Usage: grep pattern file...");
        return -1;
    }
    
    char *pattern = argv[1];
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    int match_count = 0;
    
    for (int i = 2; i < argc; i++) {
        char *filename = argv[i];
        
        FRESULT fr = f_open(&USERFile, filename, FA_READ);
        if (fr != FR_OK) {
            SHELL_LOG_USER_ERROR("grep: %s: No such file", filename);
            continue;
        }
        
        char line_buffer[256];
        int line_num = 1;
        int pos = 0;
        
        char ch;
        UINT bytes_read;
        
        while (1) {
            fr = f_read(&USERFile, &ch, 1, &bytes_read);
            if (fr != FR_OK || bytes_read == 0) break;
            
            if (ch == '\n' || pos >= sizeof(line_buffer) - 1) {
                line_buffer[pos] = '\0';
                
                if (strstr(line_buffer, pattern) != NULL) {
                    if (argc > 3) {
                        SHELL_LOG_USER_INFO("%s:%d:%s", filename, line_num, line_buffer);
                    } else {
                        SHELL_LOG_USER_INFO("%s", line_buffer);
                    }
                    match_count++;
                }
                
                pos = 0;
                line_num++;
            } else {
                line_buffer[pos++] = ch;
            }
        }
        
        f_close(&USERFile);
    }
    
    
    
    return (match_count > 0) ? 0 : 1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 grep, cmd_grep, search text patterns in files);

/* file命令 - 检测文件类型 */
int cmd_file(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("file: missing file operand");
        SHELL_LOG_USER_INFO("Usage: file file...");
        return -1;
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    for (int i = 1; i < argc; i++) {
        char *filename = argv[i];
        
        FILINFO fno;
        FRESULT fr = f_stat(filename, &fno);
        if (fr != FR_OK) {
            SHELL_LOG_USER_INFO("%s: cannot open (No such file)", filename);
            continue;
        }
        
        if (fno.fattrib & AM_DIR) {
            SHELL_LOG_USER_INFO("%s: directory", filename);
            continue;
        }
        
        // 简单的文件类型检测
        char *file_type = "data";
        char *ext = strrchr(filename, '.');
        
        if (ext) {
            if (my_strcasecmp(ext, ".txt") == 0) file_type = "ASCII text";
            else if (my_strcasecmp(ext, ".log") == 0) file_type = "ASCII text";
            else if (my_strcasecmp(ext, ".c") == 0) file_type = "C source";
            else if (my_strcasecmp(ext, ".h") == 0) file_type = "C header";
            else if (my_strcasecmp(ext, ".jpg") == 0 || my_strcasecmp(ext, ".jpeg") == 0) file_type = "JPEG image";
            else if (my_strcasecmp(ext, ".png") == 0) file_type = "PNG image";
            else if (my_strcasecmp(ext, ".bin") == 0) file_type = "binary data";
        }
        
        SHELL_LOG_USER_INFO("%s: %s, %lu bytes", filename, file_type, fno.fsize);
    }
    
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 file, cmd_file, determine file type);

/* du命令 - 显示目录磁盘使用情况 */
int cmd_du(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    char *path = ".";  // 默认当前目录
    if (argc >= 2) {
        path = argv[1];
    }
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    DIR dir;
    FILINFO fno;
    
    FRESULT fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("du: cannot access '%s'", path);
        
        return -1;
    }
    
    FSIZE_t total_size = 0;
    
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        
        if (!(fno.fattrib & AM_DIR)) {
            total_size += fno.fsize;
        }
    }
    
    SHELL_LOG_USER_INFO("%lu\t%s", (total_size + 1023) / 1024, path);  // 显示KB
    
    f_closedir(&dir);
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 du, cmd_du, estimate file space usage);

/* help命令 - 显示文件系统命令帮助 */
int cmd_fshelp(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_USER_INFO("=== STM32H725 File System Commands ===");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Directory Operations:");
    SHELL_LOG_USER_INFO("  ls [-al] [dir]       - list directory contents");
    SHELL_LOG_USER_INFO("  cd [directory]       - change directory");
    SHELL_LOG_USER_INFO("  pwd                  - print working directory");
    SHELL_LOG_USER_INFO("  mkdir [-p] dir...    - create directories");
    SHELL_LOG_USER_INFO("  rm [-rf] file...     - remove files and directories");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("File Operations:");
    SHELL_LOG_USER_INFO("  touch file...        - create empty files");
    SHELL_LOG_USER_INFO("  cp [-r] src dest     - copy files");
    SHELL_LOG_USER_INFO("  mv src dest          - move/rename files");
    SHELL_LOG_USER_INFO("  cat file...          - display file contents");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Text Processing:");
    SHELL_LOG_USER_INFO("  head [-n NUM] file   - show first lines of file");
    SHELL_LOG_USER_INFO("  tail [-n NUM] file   - show last lines of file");
    SHELL_LOG_USER_INFO("  wc [-lwc] file...    - count lines, words, chars");
    SHELL_LOG_USER_INFO("  grep pattern file... - search text patterns");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("System Information:");
    SHELL_LOG_USER_INFO("  df                   - show disk space usage");
    SHELL_LOG_USER_INFO("  du [directory]       - show directory space usage");
    SHELL_LOG_USER_INFO("  file file...         - determine file type");
    SHELL_LOG_USER_INFO("  find path -name pat  - search for files");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("SD Card Specific:");
    SHELL_LOG_USER_INFO("  sdwrite file content - write text to SD file");
    SHELL_LOG_USER_INFO("  sdread file          - read text from SD file");
    SHELL_LOG_USER_INFO("  sdls [directory]     - list SD directory (detailed)");
    SHELL_LOG_USER_INFO("  sdrm file            - remove SD file");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Examples:");
    SHELL_LOG_USER_INFO("  ls -la /");
    SHELL_LOG_USER_INFO("  mkdir -p /logs/system");
    SHELL_LOG_USER_INFO("  sdwrite hello.txt \"Hello World!\"");
    SHELL_LOG_USER_INFO("  cat hello.txt");
    SHELL_LOG_USER_INFO("  grep \"error\" system.log");
    SHELL_LOG_USER_INFO("  find / -name \"*.txt\"");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 fshelp_old, cmd_fshelp, show basic filesystem commands help);

/* echo命令 - 输出文本 */
int cmd_echo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    uint8_t no_newline = 0;
    int start_idx = 1;
    char *redirect_file = NULL;
    uint8_t redirect_append = 0;
    int redirect_idx = -1;
    
    // 解析-n选项（不输出换行符）
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        no_newline = 1;
        start_idx = 2;
    }
    
    // 查找重定向符号 > 或 >>
    for (int i = start_idx; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            if (i + 1 < argc) {
                redirect_file = argv[i + 1];
                redirect_append = 0;
                redirect_idx = i;
                break;
            }
        } else if (strcmp(argv[i], ">>") == 0) {
            if (i + 1 < argc) {
                redirect_file = argv[i + 1];
                redirect_append = 1;
                redirect_idx = i;
                break;
            }
        }
    }

    // 如果有重定向，写入文件
    if (redirect_file) {
        // 挂载文件系统
        if (check_filesystem_ready() != 0) { return -1; }

        // 打开文件（覆盖或追加模式）
        BYTE mode = redirect_append ? (FA_WRITE | FA_OPEN_ALWAYS) : (FA_WRITE | FA_CREATE_ALWAYS);
        FRESULT fr = f_open(&USERFile, redirect_file, mode);
        if (fr != FR_OK) {
            SHELL_LOG_USER_ERROR("echo: cannot create '%s': %d", redirect_file, fr);
            
            return -1;
        }

        // 如果是追加模式，移动到文件末尾
        if (redirect_append) {
            f_lseek(&USERFile, f_size(&USERFile));
        }

        // 输出所有参数到文件，用空格分隔（不包括重定向符号及其后面的内容）
        for (int i = start_idx; i < redirect_idx; i++) {
            UINT bytes_written;
            f_write(&USERFile, argv[i], strlen(argv[i]), &bytes_written);
            if (i < redirect_idx - 1) {
                f_write(&USERFile, " ", 1, &bytes_written);
            }
        }

        // 对于覆盖模式(>)，正常处理换行
        // 对于追加模式(>>)，默认不换行（直接追加到现有内容后面）
        if (!redirect_append) {
            // 覆盖模式：按照-n选项决定是否换行
            if (!no_newline) {
                UINT bytes_written;
                f_write(&USERFile, "\r\n", 2, &bytes_written);
            }
        }
        // 追加模式：默认不添加换行符，除非用户明确需要换行（通过其他方式）
        f_close(&USERFile);
        
    } else {
        // 正常输出到终端
        for (int i = start_idx; i < argc; i++) {
            shellWriteString(shell, argv[i]);
            if (i < argc - 1) {
                shellWriteString(shell, " ");
            }
        }
        
        // 输出换行符（除非使用了-n选项）
        if (!no_newline) {
            shellWriteString(shell, "\r\n");
        }
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 echo, cmd_echo, display a line of text);

/* which命令 - 显示命令位置 */
int cmd_which(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("which: missing command name");
        SHELL_LOG_USER_INFO("Usage: which command");
        return -1;
    }
    
    char *command = argv[1];

    // 搜索命令表中的命令
    ShellCommand *cmd_base = (ShellCommand *)shell->commandList.base;
    int count = shell->commandList.count;
    
    for (int i = 0; i < count; i++) {
        if (cmd_base[i].attr.attrs.type == SHELL_TYPE_CMD_MAIN ||
            cmd_base[i].attr.attrs.type == SHELL_TYPE_CMD_FUNC) {
            if (strcmp(cmd_base[i].data.cmd.name, command) == 0) {
                SHELL_LOG_USER_INFO("%s: shell builtin", command);
                return 0;
            }
        }
    }
    
    SHELL_LOG_USER_ERROR("%s: not found", command);
    return 1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 which, cmd_which, locate a command);

/* =================================================================== */
/* Tab Auto-completion Functions - Simplified Version                 */
/* =================================================================== */

/* 简化Tab补全处理函数 - 仅显示帮助信息 */
int shell_tab_completion(Shell *shell, char *buffer, int cursor_pos, int buffer_size)
{
    if (!shell || !buffer || cursor_pos < 0) return cursor_pos;

    // 简单实现：显示所有可用命令
    shellWriteString(shell, "\r\n=== Available Commands ===\r\n");
    
    ShellCommand *cmd_base = (ShellCommand *)shell->commandList.base;
    int count = shell->commandList.count;
    int cmd_count = 0;
    
    for (int i = 0; i < count; i++) {
        if ((cmd_base[i].attr.attrs.type == SHELL_TYPE_CMD_MAIN ||
             cmd_base[i].attr.attrs.type == SHELL_TYPE_CMD_FUNC) &&
            cmd_base[i].data.cmd.name) {
            
            if (cmd_count % 4 == 0 && cmd_count > 0) {
                shellWriteString(shell, "\r\n");
            }
            
            char display[20];
            snprintf(display, sizeof(display), "%-18s", cmd_base[i].data.cmd.name);
            shellWriteString(shell, display);
            cmd_count++;
            
            if (cmd_count >= 40) {  // 限制显示数量
                shellWriteString(shell, "\r\n... and more (type 'help' for full list)\r\n");
                break;
            }
        }
    }
    
    if (cmd_count % 4 != 0) {
        shellWriteString(shell, "\r\n");
    }
    
    shellWriteString(shell, "\r\nTip: Use 'help' for detailed command information\r\n");
    
    return cursor_pos;
}

/* 设置Tab补全回调函数 */
void shell_set_tab_completion(Shell *shell)
{
    if (shell) {
        // 注意: 这需要shell库支持自定义Tab处理
        // 如果shell库不支持，可能需要修改shell库源代码
        // shell->tabCompletionCallback = shell_tab_completion;
        SHELL_LOG_USER_INFO("Tab completion initialized (basic version)");
    }
}

/* =================================================================== */
/* Utility Commands                                                   */
/* =================================================================== */

/* history命令 - 显示命令历史 */
int cmd_history(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_USER_INFO("Command history feature not implemented");
    SHELL_LOG_USER_INFO("This would show the command history if supported by shell");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 history, cmd_history, display command history);

/* alias命令 - 设置命令别名 */
int cmd_alias(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    if (argc == 1) {
        // 显示当前别名
        SHELL_LOG_USER_INFO("Current aliases:");
        SHELL_LOG_USER_INFO("ll='ls -la'");
        SHELL_LOG_USER_INFO("la='ls -a'");
        SHELL_LOG_USER_INFO("dir='ls'");
        return 0;
    }
    
    SHELL_LOG_USER_INFO("Alias setting not implemented");
    SHELL_LOG_USER_INFO("Usage: alias [name='value']");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 alias, cmd_alias, create command aliases);

/* tree命令 - 显示目录树 */
int cmd_tree(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    char *path = "/";
    if (argc >= 2) {
        path = argv[1];
    }
    
    SHELL_LOG_USER_INFO("Directory tree for: %s", path);
    
    // 挂载文件系统
    if (check_filesystem_ready() != 0) { return -1; }
    
    // 简化的树形显示实现
    DIR dir;
    FILINFO fno;
    
    FRESULT fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        SHELL_LOG_USER_ERROR("tree: cannot access '%s'", path);
        
        return -1;
    }
    
    SHELL_LOG_USER_INFO("%s", path);
    
    int file_count = 0;
    int dir_count = 0;
    
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        
        if (fno.fname[0] == '.') continue;  // 跳过隐藏文件
        
        if (fno.fattrib & AM_DIR) {
            SHELL_LOG_USER_INFO("├── %s/", fno.fname);
            dir_count++;
        } else {
            SHELL_LOG_USER_INFO("├── %s", fno.fname);
            file_count++;
        }
    }
    
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("%d directories, %d files", dir_count, file_count);
    
    f_closedir(&dir);
    
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 tree, cmd_tree, display directory tree);

/* uptime命令 - 显示系统运行时间 */
int cmd_uptime(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    uint32_t tick = HAL_GetTick();
    uint32_t seconds = tick / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    if (days > 0) {
        SHELL_LOG_USER_INFO("up %lu days, %lu hours, %lu minutes", days, hours, minutes);
    } else if (hours > 0) {
        SHELL_LOG_USER_INFO("up %lu hours, %lu minutes", hours, minutes);
    } else if (minutes > 0) {
        SHELL_LOG_USER_INFO("up %lu minutes", minutes);
    } else {
        SHELL_LOG_USER_INFO("up %lu seconds", seconds);
    }
    
    // 显示内存使用情况
    size_t free_heap = xPortGetFreeHeapSize();
    SHELL_LOG_USER_INFO("Memory: %u KB free", free_heap / 1024);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 uptime, cmd_uptime, show system uptime and load);

/* date命令 - 显示日期和时间 */
int cmd_date(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    uint32_t tick = HAL_GetTick();
    uint32_t total_seconds = tick / 1000;
    
    // 简单的时间计算（从系统启动开始）
    uint32_t seconds = total_seconds % 60;
    uint32_t minutes = (total_seconds / 60) % 60;
    uint32_t hours = (total_seconds / 3600) % 24;
    uint32_t days = total_seconds / 86400;
    
    SHELL_LOG_USER_INFO("System uptime: %lu days, %02lu:%02lu:%02lu", 
                        days, hours, minutes, seconds);
    SHELL_LOG_USER_INFO("Note: Real-time clock not implemented");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 date, cmd_date, display or set system date);

/* free命令 - 显示内存使用情况 */
int cmd_free(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    // 安全地获取内存信息
    size_t free_heap = 0;
    size_t min_free_heap = 0;
    size_t total_heap = 0;
    
    // 使用安全的方式获取内存信息
    __disable_irq();  // 禁用中断以确保原子性
    
    free_heap = xPortGetFreeHeapSize();
    min_free_heap = xPortGetMinimumEverFreeHeapSize();
    
    // 尝试获取总堆大小，如果宏不可用则使用估算值
    #ifdef configTOTAL_HEAP_SIZE
        total_heap = configTOTAL_HEAP_SIZE;
    #else
        // 估算总堆大小：当前可用 + 最小历史可用，这是一个保守估算
        total_heap = free_heap + (free_heap - min_free_heap);
        if (total_heap < free_heap) {
            total_heap = free_heap + 32768;  // 32KB作为默认估算
        }
    #endif
    
    __enable_irq();   // 重新启用中断
    
    // 安全地计算已使用内存
    size_t used_heap = 0;
    if (total_heap > free_heap) {
        used_heap = total_heap - free_heap;
    }
    
    SHELL_LOG_USER_INFO("===========================================");
    SHELL_LOG_USER_INFO("         MCU 内存使用信息统计");
    SHELL_LOG_USER_INFO("===========================================");
    SHELL_LOG_USER_INFO("               total        used        free      shared  buff/cache   available");
    SHELL_LOG_USER_INFO("Mem:       %8lu   %8lu   %8lu           0           0   %8lu", 
                        (unsigned long)total_heap, 
                        (unsigned long)used_heap, 
                        (unsigned long)free_heap, 
                        (unsigned long)free_heap);
    SHELL_LOG_USER_INFO("Low Mem:   %8lu", (unsigned long)min_free_heap);
    SHELL_LOG_USER_INFO("");
    
    // 安全地计算使用百分比
    if (total_heap > 0) {
        float usage_percent = ((float)used_heap / (float)total_heap) * 100.0f;
        SHELL_LOG_USER_INFO("Memory usage: %.1f%%", (double)usage_percent);
    } else {
        SHELL_LOG_USER_INFO("Memory usage: unknown");
    }
    
    SHELL_LOG_USER_INFO("===========================================");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 free, cmd_free, display amount of free and used memory);

/* =================================================================== */
/* Common Aliases and Convenience Commands                            */
/* =================================================================== */

/* ll命令 - ls -la的别名 */
int cmd_ll(int argc, char *argv[])
{
    // 构造新的参数数组
    char *new_argv[] = {"ls", "-la", NULL};
    int new_argc = 2;

    // 添加用户提供的其他参数
    if (argc > 1) {
        new_argv[2] = argv[1];
        new_argc = 3;
    }
    
    return cmd_ls(new_argc, new_argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 ll, cmd_ll, list directory contents in long format);

/* la命令 - ls -a的别名 */
int cmd_la(int argc, char *argv[])
{
    char *new_argv[] = {"ls", "-a", NULL};
    int new_argc = 2;
    
    if (argc > 1) {
        new_argv[2] = argv[1];
        new_argc = 3;
    }
    
    return cmd_ls(new_argc, new_argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 la, cmd_la, list all files including hidden ones);

/* dir命令 - ls的别名（Windows风格） */
int cmd_dir(int argc, char *argv[])
{
    return cmd_ls(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 dir, cmd_dir, list directory contents (Windows style));

/* md命令 - mkdir的别名（Windows风格） */
int cmd_md(int argc, char *argv[])
{
    return cmd_mkdir(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 md, cmd_md, create directory (Windows style));

/* rd命令 - rm -r的别名（Windows风格） */
int cmd_rd(int argc, char *argv[])
{
    if (argc < 2) {
        SHELL_LOG_USER_ERROR("rd: missing directory name");
        return -1;
    }
    
    char *new_argv[] = {"rm", "-r", argv[1], NULL};
    return cmd_rm(3, new_argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 rd, cmd_rd, remove directory (Windows style));

/* del命令 - rm的别名（Windows风格） */
int cmd_del(int argc, char *argv[])
{
    return cmd_rm(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 del, cmd_del, delete file (Windows style));

/* type命令 - cat的别名（Windows风格） */
int cmd_type(int argc, char *argv[])
{
    return cmd_cat(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 type, cmd_type, display file contents (Windows style));

/* more命令 - cat的简化版本 */
int cmd_more(int argc, char *argv[])
{
    return cmd_cat(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 more, cmd_more, display file contents page by page);

/* less命令 - cat的别名 */
int cmd_less(int argc, char *argv[])
{
    return cmd_cat(argc, argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 less, cmd_less, view file contents);

/* 更新fshelp命令以包含新增的命令 - 重命名为避免冲突 */
int cmd_fshelp_extended(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    SHELL_LOG_USER_INFO("=== STM32H725 Extended File System Commands ===");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Directory Operations:");
    SHELL_LOG_USER_INFO("  ls [-al] [dir]       - list directory contents");
    SHELL_LOG_USER_INFO("  ll [dir]             - list in long format (ls -la)");
    SHELL_LOG_USER_INFO("  la [dir]             - list all files (ls -a)");
    SHELL_LOG_USER_INFO("  dir [dir]            - list directory (Windows style)");
    SHELL_LOG_USER_INFO("  cd [directory]       - change directory");
    SHELL_LOG_USER_INFO("  pwd                  - print working directory");
    SHELL_LOG_USER_INFO("  mkdir [-p] dir...    - create directories");
    SHELL_LOG_USER_INFO("  md dir               - create directory (Windows style)");
    SHELL_LOG_USER_INFO("  rm [-rf] file...     - remove files and directories");
    SHELL_LOG_USER_INFO("  del file             - delete file (Windows style)");
    SHELL_LOG_USER_INFO("  rd dir               - remove directory (Windows style)");
    SHELL_LOG_USER_INFO("  tree [dir]           - display directory tree");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("File Operations:");
    SHELL_LOG_USER_INFO("  touch file...        - create empty files");
    SHELL_LOG_USER_INFO("  cp [-r] src dest     - copy files");
    SHELL_LOG_USER_INFO("  mv src dest          - move/rename files");
    SHELL_LOG_USER_INFO("  cat file...          - display file contents");
    SHELL_LOG_USER_INFO("  type file            - display file contents (Windows style)");
    SHELL_LOG_USER_INFO("  more file            - view file page by page");
    SHELL_LOG_USER_INFO("  less file            - view file contents");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Text Processing:");
    SHELL_LOG_USER_INFO("  head [-n NUM] file   - show first lines of file");
    SHELL_LOG_USER_INFO("  tail [-n NUM] file   - show last lines of file");
    SHELL_LOG_USER_INFO("  wc [-lwc] file...    - count lines, words, chars");
    SHELL_LOG_USER_INFO("  grep pattern file... - search text patterns");
    SHELL_LOG_USER_INFO("  echo [-n] text...    - display text");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("System Information:");
    SHELL_LOG_USER_INFO("  df                   - show disk space usage");
    SHELL_LOG_USER_INFO("  du [directory]       - show directory space usage");
    SHELL_LOG_USER_INFO("  free                 - show memory usage");
    SHELL_LOG_USER_INFO("  uptime               - show system uptime");
    SHELL_LOG_USER_INFO("  date                 - show system time");
    SHELL_LOG_USER_INFO("  file file...         - determine file type");
    SHELL_LOG_USER_INFO("  find path -name pat  - search for files");
    SHELL_LOG_USER_INFO("  which command        - locate command");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Utilities:");
    SHELL_LOG_USER_INFO("  history              - show command history");
    SHELL_LOG_USER_INFO("  alias [name=value]   - create command aliases");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("SD Card Specific:");
    SHELL_LOG_USER_INFO("  sdwrite file content - write text to SD file");
    SHELL_LOG_USER_INFO("  sdread file          - read text from SD file");
    SHELL_LOG_USER_INFO("  sdls [directory]     - list SD directory (detailed)");
    SHELL_LOG_USER_INFO("  sdrm file            - remove SD file");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Tab Completion:");
    SHELL_LOG_USER_INFO("  Press TAB to auto-complete commands and filenames");
    SHELL_LOG_USER_INFO("");
    SHELL_LOG_USER_INFO("Examples:");
    SHELL_LOG_USER_INFO("  ll /                 # List all files in root");
    SHELL_LOG_USER_INFO("  mkdir -p logs/debug  # Create nested directories");
    SHELL_LOG_USER_INFO("  echo \"Hello World\" > hello.txt  # (if redirection supported)");
    SHELL_LOG_USER_INFO("  sdwrite test.txt \"Hello STM32!\"");
    SHELL_LOG_USER_INFO("  grep \"error\" *.log");
    SHELL_LOG_USER_INFO("  find / -name \"*.txt\"");
    SHELL_LOG_USER_INFO("  tree /");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 fshelp, cmd_fshelp_extended, comprehensive help for all file system commands);

/* =================================================================== */
/* Audio Recording Commands                                           */
/* =================================================================== */

/**
 * @brief Start audio recording command
 * @param argc argument count
 * @param argv argument vector
 * @return int command result
 */
int cmd_audio_start(int argc, char *argv[])
{
    // 移除重复的状态检查，让audio_recorder_start()内部处理
    // audio_recorder_start()已经包含了完整的状态检查和重置逻辑
    
    if (audio_recorder_start() == 0) {
        SHELL_LOG_USER_INFO("Audio recording started");
    SHELL_LOG_USER_INFO("Format: %luch_%lubit_%luHz",
                 (unsigned long)audio_recorder_get_channel_count(),
                 (unsigned long)audio_recorder_get_bit_depth(),
                 (unsigned long)audio_recorder_get_sample_rate());
        SHELL_LOG_USER_INFO("File: %s", audio_recorder_get_filename());
    } else {
        SHELL_LOG_USER_ERROR("Failed to start audio recording");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Stop audio recording command
 * @param argc argument count
 * @param argv argument vector
 * @return int command result
 */
int cmd_audio_stop(int argc, char *argv[])
{
    // 移除重复的状态检查，让audio_recorder_stop()内部处理
    // audio_recorder_stop()已经包含了完整的状态检查和错误处理逻辑
    
    if (audio_recorder_stop() == 0) {
        SHELL_LOG_USER_INFO("Audio recording stopped");
        SHELL_LOG_USER_INFO("Total bytes written: %lu", audio_recorder_get_bytes_written());
    } else {
        SHELL_LOG_USER_ERROR("Failed to stop audio recording");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Reset audio recorder to clean state
 * @param argc argument count
 * @param argv argument vector
 * @return int command result
 */
int cmd_audio_reset(int argc, char *argv[])
{
    if (audio_recorder_reset() == 0) {
        SHELL_LOG_USER_INFO("Audio recorder reset to clean state");
        SHELL_LOG_USER_INFO("Current state: %d (IDLE=0)", audio_recorder_get_state());
    } else {
        SHELL_LOG_USER_ERROR("Failed to reset audio recorder");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Measure external I2S/TDM clock frequency to diagnose timing issues
 * @param argc argument count
 * @param argv argument vector
 * @return int command result
 */
int cmd_audio_measure_clock(int argc, char *argv[])
{
    SHELL_LOG_USER_INFO("Starting external clock frequency measurement...");
    SHELL_LOG_USER_INFO("This will take ~5-10 seconds to complete");
    
    audio_recorder_measure_clock();
    
    return 0;
}

/**
 * @brief Show audio recording status command
 * @param argc argument count
 * @param argv argument vector
 * @return int command result
 */
int cmd_audio_status(int argc, char *argv[])
{
    AudioRecorderState_t state = audio_recorder_get_state();
    
    SHELL_LOG_USER_INFO("Audio Recorder Status:");
    SHELL_LOG_USER_INFO("  Format: 8ch_16bit_48000Hz");
    
    switch (state) {
        case AUDIO_REC_IDLE:
            SHELL_LOG_USER_INFO("  State: IDLE");
            break;
        case AUDIO_REC_RECORDING:
            SHELL_LOG_USER_INFO("  State: RECORDING");
            SHELL_LOG_USER_INFO("  File: %s", audio_recorder_get_filename());
            SHELL_LOG_USER_INFO("  Bytes written: %lu", audio_recorder_get_bytes_written());
            break;
        case AUDIO_REC_STOPPING:
            SHELL_LOG_USER_INFO("  State: STOPPING");
            break;
        case AUDIO_REC_ERROR:
            SHELL_LOG_USER_INFO("  State: ERROR");
            break;
        default:
            SHELL_LOG_USER_INFO("  State: UNKNOWN");
            break;
    }
    audio_recorder_debug_status();
    audio_recorder_check_sd_card();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 audio_start, cmd_audio_start, start I2S TDM audio recording to SD card);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 audio_stop, cmd_audio_stop, stop audio recording);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 audio_reset, cmd_audio_reset, reset audio recorder to clean state);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 audio_status, cmd_audio_status, show audio recording status);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 audio_measure_clock, cmd_audio_measure_clock, measure external I2S/TDM clock frequency);

/* =================================================================== */
/* File Write Performance Test Commands                               */
/* =================================================================== */

/**
 * @brief 测试循环写入4096字节数据到文件
 * @param argc 参数个数
 * @param argv 参数数组
 * @retval 0: 成功, -1: 失败
 */
int cmd_test_write_loop(int argc, char **argv)
{
    int loops = 100;  // 默认写入100次
    int sync_interval = 8;  // 默认每8次写入同步一次
    const char* filename = "test_write_loop.bin";
    
    // 解析命令行参数
    if (argc >= 2) {
        loops = atoi(argv[1]);
        if (loops <= 0 || loops > 10000) {
            SHELL_LOG_USER_ERROR("Invalid loop count: %d (valid range: 1-10000)", loops);
            return -1;
        }
    }
    
    if (argc >= 3) {
        sync_interval = atoi(argv[2]);
        if (sync_interval <= 0 || sync_interval > loops) {
            SHELL_LOG_USER_ERROR("Invalid sync interval: %d (valid range: 1-%d)", sync_interval, loops);
            return -1;
        }
    }
    
    if (argc >= 4) {
        filename = argv[3];
    }
    
    SHELL_LOG_USER_INFO("Starting write loop test:");
    SHELL_LOG_USER_INFO("  Loops: %d", loops);
    SHELL_LOG_USER_INFO("  Data size per loop: 4096 bytes");
    SHELL_LOG_USER_INFO("  Total data: %lu bytes", (unsigned long)(loops * 4096));
    SHELL_LOG_USER_INFO("  Sync interval: every %d writes", sync_interval);
    SHELL_LOG_USER_INFO("  Filename: %s", filename);
    
    // 检查文件系统状态
    if (MOUNT_FILESYSTEM() != 0) {
        return -1;
    }
    
    // 动态分配测试数据缓冲区 (避免栈溢出)
    uint8_t* test_data = (uint8_t*)pvPortMalloc(4096);
    if (test_data == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate 4096 bytes for test data");
        return -1;
    }
    
    // 创建测试数据模式 (4096字节)
    for (int i = 0; i < 4096; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);  // 递增模式
    }
    
    // 动态分配文件对象 (避免静态变量的线程安全问题)
    FIL* test_file = (FIL*)pvPortMalloc(sizeof(FIL));
    if (test_file == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate memory for file object");
        vPortFree(test_data);
        return -1;
    }
    
    // 清零文件对象以确保干净的初始状态
    memset(test_file, 0, sizeof(FIL));
    
    FRESULT res = f_open(test_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to open file %s, FRESULT: %d", filename, res);
        vPortFree(test_data);  // 释放内存
        vPortFree(test_file);  // 释放文件对象内存
        return -1;
    }
    
    // 验证文件对象初始状态
    SHELL_LOG_USER_INFO("File opened successfully, initial object state:");
    SHELL_LOG_USER_INFO("  File object fs pointer: %p", test_file->obj.fs);
    SHELL_LOG_USER_INFO("  File object id: %d", test_file->obj.id);
    SHELL_LOG_USER_INFO("  File flag: 0x%02X", test_file->flag);
    SHELL_LOG_USER_INFO("  File error: %d", test_file->err);
    
    SHELL_LOG_USER_INFO("Starting write test...");
    
    // 记录开始时间
    uint32_t start_time = HAL_GetTick();
    uint32_t total_bytes_written = 0;
    int write_errors = 0;
    int sync_errors = 0;
    
    // 循环写入测试
    for (int i = 0; i < loops; i++) {
        UINT bytes_written;
        
        // 验证文件对象有效性和内存完整性
        if (test_file->obj.fs == NULL) {
            SHELL_LOG_USER_ERROR("File object fs pointer is NULL at loop %d, aborting", i + 1);
            write_errors += 10;
            break;
        }
        
        // 额外的文件对象健康检查
        if (test_file->flag == 0xFF || test_file->obj.id == 0xFFFF) {
            SHELL_LOG_USER_ERROR("File object appears corrupted at loop %d", i + 1);
            SHELL_LOG_USER_ERROR("  Flag: 0x%02X, ID: %d", test_file->flag, test_file->obj.id);
            write_errors += 10;
            break;
        }
        
        // 记录写入前的文件对象状态
        SHELL_LOG_USER_DEBUG("Before write - fs: %p, id: %d, flag: 0x%02X", 
                            test_file->obj.fs, test_file->obj.id, test_file->flag);
        
        // 写入4096字节
        res = f_write(test_file, test_data, 4096, &bytes_written);
        
        // 记录写入后的文件对象状态
        SHELL_LOG_USER_DEBUG("After write - fs: %p, id: %d, flag: 0x%02X, res: %d", 
                            test_file->obj.fs, test_file->obj.id, test_file->flag, res);
        
        if (res != FR_OK || bytes_written != 4096) {
            write_errors++;
            SHELL_LOG_USER_ERROR("Write error at loop %d: FRESULT=%d, written=%lu", 
                                i + 1, res, (unsigned long)bytes_written);
            
            // 当遇到 FR_INVALID_OBJECT 错误时，进行详细诊断
            if (res == FR_INVALID_OBJECT) {
                SHELL_LOG_USER_ERROR("=== FILE OBJECT DIAGNOSTIC ===");
                SHELL_LOG_USER_ERROR("File object fs pointer: %p", test_file->obj.fs);
                SHELL_LOG_USER_ERROR("File object id: %d", test_file->obj.id);
                SHELL_LOG_USER_ERROR("File flag: 0x%02X", test_file->flag);
                SHELL_LOG_USER_ERROR("File error: %d", test_file->err);
                SHELL_LOG_USER_ERROR("File pointer: %lu", (unsigned long)test_file->fptr);
                SHELL_LOG_USER_ERROR("Test data pointer: %p", test_data);
                SHELL_LOG_USER_ERROR("Loop iteration: %d", i + 1);
            }
            
            // 详细错误分析
            switch(res) {
                case FR_DISK_ERR:
                    SHELL_LOG_USER_ERROR("  -> Disk I/O error");
                    break;
                case FR_INT_ERR:
                    SHELL_LOG_USER_ERROR("  -> Internal error");
                    break;
                case FR_NOT_READY:
                    SHELL_LOG_USER_ERROR("  -> Drive not ready");
                    break;
                case FR_INVALID_OBJECT:
                    SHELL_LOG_USER_ERROR("  -> Invalid file object! (File structure corrupted)");
                    break;
                case FR_WRITE_PROTECTED:
                    SHELL_LOG_USER_ERROR("  -> Write protected");
                    break;
                default:
                    SHELL_LOG_USER_ERROR("  -> Unknown error code: %d", res);
                    break;
            }
            
            if (write_errors >= 5) {
                SHELL_LOG_USER_ERROR("Too many write errors, aborting test");
                break;
            }
        } else {
            total_bytes_written += bytes_written;
            
            // 确保写入操作完成 (内存屏障)
            __DSB();
            __ISB();
        }
        
        // 周期性同步
        if ((i + 1) % sync_interval == 0) {
            // 在同步前记录详细的文件对象状态
            SHELL_LOG_USER_DEBUG("Before sync - fs: %p, id: %d, flag: 0x%02X", 
                                test_file->obj.fs, test_file->obj.id, test_file->flag);
            
            // 在同步前检查文件对象状态
            if (test_file->obj.fs == NULL) {
                SHELL_LOG_USER_ERROR("File object fs pointer NULL before sync");
                sync_errors++;
                break;
            }
            
            FRESULT sync_res = f_sync(test_file);
            
            // 记录同步后的文件对象状态
            SHELL_LOG_USER_DEBUG("After sync - fs: %p, id: %d, flag: 0x%02X, sync_res: %d", 
                                test_file->obj.fs, test_file->obj.id, test_file->flag, sync_res);
            
            if (sync_res != FR_OK) {
                sync_errors++;
                // 简化日志输出以避免格式化问题
                SHELL_LOG_USER_WARNING("Sync error at loop");
                SHELL_LOG_USER_WARNING("Loop number: %d", i + 1);
                SHELL_LOG_USER_WARNING("FRESULT: %d", sync_res);
                
                // 检查同步后的文件对象状态
                if (sync_res == FR_INVALID_OBJECT) {
                    SHELL_LOG_USER_ERROR("File object became invalid after write");
                    SHELL_LOG_USER_ERROR("This suggests memory corruption");
                    break;  // 停止测试
                }
            } else {
                SHELL_LOG_USER_DEBUG("Synced at loop %d", i + 1);
            }
        }
        
        // 进度报告 (每100次或最后一次)
        if ((i + 1) % 100 == 0 || i == loops - 1) {
            uint32_t elapsed = HAL_GetTick() - start_time;
            uint32_t progress_percent = (uint32_t)((uint64_t)(i + 1) * 100 / loops);
            SHELL_LOG_USER_INFO("Progress: %lu%% (%d/%d), elapsed: %lu ms", 
                              (unsigned long)progress_percent, i + 1, loops, elapsed);
        }
    }
    
    // 最终同步和关闭文件
    SHELL_LOG_USER_INFO("Performing final sync...");
    FRESULT final_sync = f_sync(test_file);
    if (final_sync != FR_OK) {
        SHELL_LOG_USER_WARNING("Final sync failed: FRESULT=%d", final_sync);
        sync_errors++;
    }
    
    FRESULT close_res = f_close(test_file);
    if (close_res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to close file: FRESULT=%d", close_res);
    }
    
    // 计算和显示测试结果
    uint32_t total_time = HAL_GetTick() - start_time;
    
    // 避免浮点数运算，使用整数计算
    uint32_t write_speed_bps = 0;
    uint32_t success_rate_percent = 0;
    
    if (total_bytes_written > 0 && total_time > 0) {
        write_speed_bps = (uint32_t)((uint64_t)total_bytes_written * 1000 / total_time);
    }
    
    if (loops > 0) {
        success_rate_percent = (uint32_t)((uint64_t)(loops - write_errors) * 100 / loops);
    }
    
    SHELL_LOG_USER_INFO("=== Write Loop Test Results ===");
    SHELL_LOG_USER_INFO("Total time: %lu ms", total_time);
    SHELL_LOG_USER_INFO("Total bytes written: %lu", (unsigned long)total_bytes_written);
    SHELL_LOG_USER_INFO("Write speed: %lu bytes/s (%lu KB/s)", 
                       write_speed_bps, write_speed_bps / 1024);
    SHELL_LOG_USER_INFO("Write errors: %d", write_errors);
    SHELL_LOG_USER_INFO("Sync errors: %d", sync_errors);
    SHELL_LOG_USER_INFO("Success rate: %lu%%", (unsigned long)success_rate_percent);
    
    if (write_errors == 0 && sync_errors == 0) {
        SHELL_LOG_USER_INFO("Test completed successfully!");
    } else {
        SHELL_LOG_USER_WARNING("Test completed with errors");
    }
    
    // 释放测试数据内存
    vPortFree(test_data);
    
    // 释放文件对象内存
    vPortFree(test_file);
    
    return (write_errors == 0) ? 0 : -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_write_loop, cmd_test_write_loop, test loop writing 4096 bytes to file [loops] [sync_interval] [filename]);

/**
 * @brief 测试单次写入大块数据到文件 (快速测试)
 * @param argc 参数个数
 * @param argv 参数数组
 * @retval 0: 成功, -1: 失败
 */
int cmd_test_write_single(int argc, char **argv)
{
    int data_size = 4096;  // 默认4KB
    const char* filename = "test_write_single.bin";
    
    // 解析命令行参数
    if (argc >= 2) {
        data_size = atoi(argv[1]);
        if (data_size <= 0 || data_size > (512 * 1024)) {  // 最大512KB
            SHELL_LOG_USER_ERROR("Invalid data size: %d (valid range: 1-%d)", data_size, 512 * 1024);
            return -1;
        }
    }
    
    if (argc >= 3) {
        filename = argv[2];
    }
    
    SHELL_LOG_USER_INFO("Starting single write test:");
    SHELL_LOG_USER_INFO("  Data size: %d bytes", data_size);
    SHELL_LOG_USER_INFO("  Filename: %s", filename);
    
    // 检查文件系统状态
    if (MOUNT_FILESYSTEM() != 0) {
        return -1;
    }
    
    // 分配测试数据缓冲区
    uint8_t* test_data = (uint8_t*)pvPortMalloc(data_size);
    if (test_data == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate %d bytes for test data", data_size);
        return -1;
    }
    
    // 创建测试数据模式
    for (int i = 0; i < data_size; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    
    // 打开文件 (动态分配文件对象避免线程安全问题)
    FIL* test_file_single = (FIL*)pvPortMalloc(sizeof(FIL));
    if (test_file_single == NULL) {
        SHELL_LOG_USER_ERROR("Failed to allocate memory for file object");
        vPortFree(test_data);
        return -1;
    }
    
    // 清零文件对象以确保干净的初始状态
    memset(test_file_single, 0, sizeof(FIL));
    
    FRESULT res = f_open(test_file_single, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SHELL_LOG_USER_ERROR("Failed to open file %s, FRESULT: %d", filename, res);
        vPortFree(test_data);
        vPortFree(test_file_single);
        return -1;
    }
    
    // 记录开始时间并执行写入
    uint32_t start_time = HAL_GetTick();
    
    UINT bytes_written;
    res = f_write(test_file_single, test_data, data_size, &bytes_written);
    
    // 确保写入操作完成 (内存屏障)
    __DSB();
    __ISB();
    
    uint32_t write_time = HAL_GetTick() - start_time;
    
    // 同步文件
    uint32_t sync_start = HAL_GetTick();
    FRESULT sync_res = f_sync(test_file_single);
    uint32_t sync_time = HAL_GetTick() - sync_start;
    
    // 关闭文件
    FRESULT close_res = f_close(test_file_single);
    
    uint32_t total_time = HAL_GetTick() - start_time;
    
    // 释放内存
    vPortFree(test_data);
    vPortFree(test_file_single);
    
    // 显示测试结果
    SHELL_LOG_USER_INFO("=== Single Write Test Results ===");
    
    if (res == FR_OK && bytes_written == data_size) {
        SHELL_LOG_USER_INFO("Write: SUCCESS");
        SHELL_LOG_USER_INFO("  Bytes written: %lu", (unsigned long)bytes_written);
        SHELL_LOG_USER_INFO("  Write time: %lu ms", write_time);
        
        // 避免浮点数运算，使用整数计算速度 (bytes/second)
        uint32_t write_speed_bps = 0;
        if (write_time > 0) {
            write_speed_bps = (uint32_t)((uint64_t)bytes_written * 1000 / write_time);
        }
        SHELL_LOG_USER_INFO("  Write speed: %lu bytes/s (%lu KB/s)", 
                           write_speed_bps, write_speed_bps / 1024);
    } else {
        SHELL_LOG_USER_ERROR("Write: FAILED");
        SHELL_LOG_USER_ERROR("  FRESULT: %d", res);
        SHELL_LOG_USER_ERROR("  Expected: %d, Written: %lu", data_size, (unsigned long)bytes_written);
    }
    
    if (sync_res == FR_OK) {
        SHELL_LOG_USER_INFO("Sync: SUCCESS (%lu ms)", sync_time);
    } else {
        SHELL_LOG_USER_ERROR("Sync: FAILED (FRESULT: %d)", sync_res);
    }
    
    if (close_res == FR_OK) {
        SHELL_LOG_USER_INFO("Close: SUCCESS");
    } else {
        SHELL_LOG_USER_ERROR("Close: FAILED (FRESULT: %d)", close_res);
    }
    
    SHELL_LOG_USER_INFO("Total time: %lu ms", total_time);
    
    return (res == FR_OK && sync_res == FR_OK && close_res == FR_OK) ? 0 : -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_write_single, cmd_test_write_single, test single write to file [data_size] [filename]);
