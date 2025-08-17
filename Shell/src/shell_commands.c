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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    HAL_Delay(100);
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