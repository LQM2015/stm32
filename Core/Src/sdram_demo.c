#include <stdint.h>
#include "shell_log.h"  /* APP uses SHELL_LOG_MEMORY_xxx macros */

/* 将此函数链接到SDRAM执行区域 */
__attribute__((section(".text_sdram")))
void SDRAM_DemoFunction(void)
{
    volatile uint32_t acc = 0;
    for (uint32_t i = 0; i < 1024; i++) {
        acc += i * 3U + 1U;
    }
    SHELL_LOG_MEMORY_INFO("[SDRAM Demo] Function address: 0x%08X", (unsigned int)(uintptr_t)&SDRAM_DemoFunction);
    SHELL_LOG_MEMORY_INFO("[SDRAM Demo] Accumulator: %lu", acc);
}

/* 放一个常量到SDRAM只读区域 */
__attribute__((section(".rodata_sdram")))
static const char *sdram_demo_banner = "SDRAM code section is active";

__attribute__((section(".text_sdram")))
void SDRAM_DemoPrintBanner(void)
{
    SHELL_LOG_MEMORY_INFO("[SDRAM Demo] %s", sdram_demo_banner);
}