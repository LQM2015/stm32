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
    
    shellPrint(shell, "=== STM32H725 System Information ===\r\n");
    shellPrint(shell, "CPU ID: 0x%08lX\r\n", HAL_GetDEVID());
    shellPrint(shell, "CPU Rev: 0x%08lX\r\n", HAL_GetREVID());
    shellPrint(shell, "UID: %08lX-%08lX-%08lX\r\n", 
           HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    shellPrint(shell, "System Clock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    shellPrint(shell, "HCLK: %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    shellPrint(shell, "PCLK1: %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    shellPrint(shell, "PCLK2: %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
    shellPrint(shell, "Tick: %lu ms\r\n", HAL_GetTick());
    shellPrint(shell, "HAL Version: %lu\r\n", HAL_GetHalVersion());
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 sysinfo, cmd_sysinfo, show system information);

/* 内存信息命令 */
int cmd_meminfo(int argc, char *argv[])
{
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
    
    printf("=== Memory Information ===\r\n");
    printf("Free Heap: %u bytes\r\n", free_heap);
    printf("Min Free Heap: %u bytes\r\n", min_free_heap);
    printf("Used Heap: %u bytes\r\n", configTOTAL_HEAP_SIZE - free_heap);
    printf("Total Heap: %u bytes\r\n", configTOTAL_HEAP_SIZE);
    
    float usage_percent = ((float)(configTOTAL_HEAP_SIZE - free_heap) / configTOTAL_HEAP_SIZE) * 100;
    printf("Memory Usage: %.1f%%\r\n", usage_percent);
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 meminfo, cmd_meminfo, show memory information);

/* 任务信息命令 */
int cmd_taskinfo(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (!shell) return -1;
    
    shellPrint(shell, "=== FreeRTOS Task Information ===\r\n");
    shellPrint(shell, "Kernel State: %s\r\n", 
           (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? "Running" : "Not Running");
    shellPrint(shell, "Kernel Tick: %lu\r\n", xTaskGetTickCount());
    shellPrint(shell, "Kernel Frequency: %lu Hz\r\n", (uint32_t)configTICK_RATE_HZ);
    
    #if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    char *pcWriteBuffer = pvPortMalloc(1024);
    if (pcWriteBuffer != NULL) {
        vTaskList(pcWriteBuffer);
        shellPrint(shell, "Task List:\r\n%s", pcWriteBuffer);
        vPortFree(pcWriteBuffer);
    }
    #endif
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 taskinfo, cmd_taskinfo, show task information);

/* 重启命令 */
int cmd_reboot(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    if (shell) {
        shellPrint(shell, "System rebooting...\r\n");
    }
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
        shellPrint(shell, "Usage: led <on|off|toggle>\r\n");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        shellPrint(shell, "LED turned ON\r\n");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else if (strcmp(argv[1], "off") == 0) {
        shellPrint(shell, "LED turned OFF\r\n");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    } else if (strcmp(argv[1], "toggle") == 0) {
        shellPrint(shell, "LED toggled\r\n");
        // HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    } else {
        shellPrint(shell, "Invalid parameter. Use: on, off, or toggle\r\n");
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
    
    shellPrint(shell, "Starting clock profile test...\r\n");
    TestAllClockProfiles();
    shellPrint(shell, "Clock profile test completed\r\n");
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
        shellPrint(shell, "Usage: setclock <profile>\r\n");
        shellPrint(shell, "Available profiles:\r\n");
        shellPrint(shell, "  0 - 32kHz (Ultra Low Power - LSI)\r\n");
        shellPrint(shell, "  1 - 24MHz (Low Power - HSI/4)\r\n");
        shellPrint(shell, "  2 - 48MHz (Energy Saving)\r\n");
        shellPrint(shell, "  3 - 96MHz (Balanced)\r\n");
        shellPrint(shell, "  4 - 128MHz (Standard)\r\n");
        shellPrint(shell, "  5 - 200MHz (High Efficiency)\r\n");
        shellPrint(shell, "  6 - 300MHz (High Performance)\r\n");
        shellPrint(shell, "  7 - 400MHz (Ultra High Performance)\r\n");
        shellPrint(shell, "  8 - 550MHz (Maximum Performance)\r\n");
        shellPrint(shell, "Current System Clock: %lu Hz (%.1f MHz)\r\n", 
               HAL_RCC_GetSysClockFreq(), HAL_RCC_GetSysClockFreq() / 1000000.0f);
        return 0;
    }
    
    int profile = atoi(argv[1]);
    if (profile < 0 || profile > 8) {
        shellPrint(shell, "Error: Invalid profile %d. Valid range: 0-8\r\n", profile);
        return -1;
    }
    
    shellPrint(shell, "Switching to clock profile %d...\r\n", profile);
    
    // 记录切换前的时钟频率
    uint32_t old_freq = HAL_RCC_GetSysClockFreq();
    
    // 执行时钟切换
    ClockProfile_t clock_profile = (ClockProfile_t)profile;
    if (SwitchSystemClock(clock_profile) == HAL_OK) {
        // 切换成功，显示新的时钟频率
        uint32_t new_freq = HAL_RCC_GetSysClockFreq();
        shellPrint(shell, "Clock switch successful!\r\n");
        shellPrint(shell, "Previous: %lu Hz (%.1f MHz)\r\n", old_freq, old_freq / 1000000.0f);
        shellPrint(shell, "Current:  %lu Hz (%.1f MHz)\r\n", new_freq, new_freq / 1000000.0f);
        
        // 显示其他时钟域的频率
        shellPrint(shell, "HCLK:  %lu Hz (%.1f MHz)\r\n", HAL_RCC_GetHCLKFreq(), HAL_RCC_GetHCLKFreq() / 1000000.0f);
        shellPrint(shell, "PCLK1: %lu Hz (%.1f MHz)\r\n", HAL_RCC_GetPCLK1Freq(), HAL_RCC_GetPCLK1Freq() / 1000000.0f);
        shellPrint(shell, "PCLK2: %lu Hz (%.1f MHz)\r\n", HAL_RCC_GetPCLK2Freq(), HAL_RCC_GetPCLK2Freq() / 1000000.0f);
    } else {
        shellPrint(shell, "Clock switch failed!\r\n");
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
    
    shellPrint(shell, "=== Firmware Version Information ===\r\n");
    shellPrint(shell, "Shell Version: %s\r\n", SHELL_VERSION);
    shellPrint(shell, "HAL Version: %lu\r\n", HAL_GetHalVersion());
    shellPrint(shell, "FreeRTOS Version: %s\r\n", tskKERNEL_VERSION_NUMBER);
    shellPrint(shell, "Build Date: %s %s\r\n", __DATE__, __TIME__);
    shellPrint(shell, "MCU: STM32H725AEIX\r\n");
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
        shellPrint(shell, "Usage: hexdump <address> <length>\r\n");
        shellPrint(shell, "Example: hexdump 0x08000000 256\r\n");
        return -1;
    }
    
    uint32_t addr = strtoul(argv[1], NULL, 0);
    uint32_t len = strtoul(argv[2], NULL, 0);
    
    if (len > 1024) {
        shellPrint(shell, "Length too large, max 1024 bytes\r\n");
        return -1;
    }
    
    shellPrint(shell, "Hex dump from 0x%08lX, length %lu:\r\n", addr, len);
    
    uint8_t *ptr = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i += 16) {
        shellPrint(shell, "%08lX: ", addr + i);
        
        // 打印十六进制
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            shellPrint(shell, "%02X ", ptr[i + j]);
        }
        
        // 补齐空格
        for (uint32_t j = len - i; j < 16; j++) {
            shellPrint(shell, "   ");
        }
        
        shellPrint(shell, " |");
        
        // 打印ASCII
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            char c = ptr[i + j];
            shellPrint(shell, "%c", (c >= 32 && c <= 126) ? c : '.');
        }
        
        shellPrint(shell, "|\r\n");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 hexdump, cmd_hexdump, hex dump memory);

/* 变量导出示例 */
static int test_var = 12345;
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_INT), 
                 testVar, &test_var, test variable);

static char test_string[] = "Hello STM32H725!";
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_STRING), 
                 testStr, test_string, test string variable);