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
#include "clock_management.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 系统信息命令 */
int cmd_sysinfo(int argc, char *argv[])
{
    printf("=== STM32H725 System Information ===\r\n");
    printf("CPU ID: 0x%08lX\r\n", HAL_GetDEVID());
    printf("CPU Rev: 0x%08lX\r\n", HAL_GetREVID());
    printf("UID: %08lX-%08lX-%08lX\r\n", 
           HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    printf("System Clock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("HCLK: %lu Hz\r\n", HAL_RCC_GetHCLKFreq());
    printf("PCLK1: %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());
    printf("PCLK2: %lu Hz\r\n", HAL_RCC_GetPCLK2Freq());
    printf("Tick: %lu ms\r\n", HAL_GetTick());
    printf("HAL Version: %lu\r\n", HAL_GetHalVersion());
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
    printf("=== FreeRTOS Task Information ===\r\n");
    printf("Kernel State: %s\r\n", 
           (osKernelGetState() == osKernelRunning) ? "Running" : "Not Running");
    printf("Kernel Tick: %lu\r\n", osKernelGetTickCount());
    printf("Kernel Frequency: %lu Hz\r\n", osKernelGetTickFreq());
    
    #if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    char *pcWriteBuffer = pvPortMalloc(1024);
    if (pcWriteBuffer != NULL) {
        vTaskList(pcWriteBuffer);
        printf("Task List:\r\n%s", pcWriteBuffer);
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
    printf("System rebooting...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 reboot, cmd_reboot, reboot system);


/* LED控制命令（如果有LED的话） */
int cmd_led(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: led <on|off|toggle>\r\n");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        printf("LED turned ON\r\n");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else if (strcmp(argv[1], "off") == 0) {
        printf("LED turned OFF\r\n");
        // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    } else if (strcmp(argv[1], "toggle") == 0) {
        printf("LED toggled\r\n");
        // HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    } else {
        printf("Invalid parameter. Use: on, off, or toggle\r\n");
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 led, cmd_led, control LED);

/* 时钟配置测试命令 */
int cmd_clocktest(int argc, char *argv[])
{
    printf("Starting clock profile test...\r\n");
    TestAllClockProfiles();
    printf("Clock profile test completed\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 clocktest, cmd_clocktest, test all clock profiles);

/* 版本信息命令 */
int cmd_version(int argc, char *argv[])
{
    printf("=== Firmware Version Information ===\r\n");
    printf("Shell Version: %s\r\n", SHELL_VERSION);
    printf("HAL Version: %lu\r\n", HAL_GetHalVersion());
    printf("FreeRTOS Version: %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("Build Date: %s %s\r\n", __DATE__, __TIME__);
    printf("MCU: STM32H725AEIX\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 version, cmd_version, show version information);

/* 十六进制dump命令 */
int cmd_hexdump(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: hexdump <address> <length>\r\n");
        printf("Example: hexdump 0x08000000 256\r\n");
        return -1;
    }
    
    uint32_t addr = strtoul(argv[1], NULL, 0);
    uint32_t len = strtoul(argv[2], NULL, 0);
    
    if (len > 1024) {
        printf("Length too large, max 1024 bytes\r\n");
        return -1;
    }
    
    printf("Hex dump from 0x%08lX, length %lu:\r\n", addr, len);
    
    uint8_t *ptr = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i += 16) {
        printf("%08lX: ", addr + i);
        
        // 打印十六进制
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            printf("%02X ", ptr[i + j]);
        }
        
        // 补齐空格
        for (uint32_t j = len - i; j < 16; j++) {
            printf("   ");
        }
        
        printf(" |");
        
        // 打印ASCII
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            char c = ptr[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        
        printf("|\r\n");
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