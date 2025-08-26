#include "main.h"
#include "shell_log.h"
#include "FreeRTOS.h"
#include "task.h"

// 故障状态寄存器地址定义
#define CFSR    (*((volatile unsigned long *)(0xE000ED28)))
#define HFSR    (*((volatile unsigned long *)(0xE000ED2C)))
#define DFSR    (*((volatile unsigned long *)(0xE000ED30)))
#define AFSR    (*((volatile unsigned long *)(0xE000ED3C)))
#define BFAR    (*((volatile unsigned long *)(0xE000ED38)))
#define MMAR    (*((volatile unsigned long *)(0xE000ED34)))

// CFSR 寄存器位域定义
#define CFSR_MMFSR_MASK  0x000000FF  // Memory Management Fault Status Register
#define CFSR_BFSR_MASK   0x0000FF00  // Bus Fault Status Register
#define CFSR_UFSR_MASK   0xFFFF0000  // Usage Fault Status Register

// Memory Management Fault Status Register 位定义
#define MMFSR_IACCVIOL   (1 << 0)    // 指令访问违规
#define MMFSR_DACCVIOL   (1 << 1)    // 数据访问违规
#define MMFSR_MUNSTKERR  (1 << 3)    // 异常返回时栈操作错误
#define MMFSR_MSTKERR    (1 << 4)    // 异常入口时栈操作错误
#define MMFSR_MLSPERR    (1 << 5)    // 浮点单元延迟状态保存错误
#define MMFSR_MMARVALID  (1 << 7)    // MMAR 寄存器有效

// Bus Fault Status Register 位定义
#define BFSR_IBUSERR     (1 << 8)    // 指令总线错误
#define BFSR_PRECISERR   (1 << 9)    // 精确数据总线错误
#define BFSR_IMPRECISERR (1 << 10)   // 非精确数据总线错误
#define BFSR_UNSTKERR    (1 << 11)   // 异常返回时栈操作错误
#define BFSR_STKERR      (1 << 12)   // 异常入口时栈操作错误
#define BFSR_LSPERR      (1 << 13)   // 浮点单元延迟状态保存错误
#define BFSR_BFARVALID   (1 << 15)   // BFAR 寄存器有效

// Usage Fault Status Register 位定义
#define UFSR_UNDEFINSTR  (1 << 16)   // 未定义指令
#define UFSR_INVSTATE    (1 << 17)   // 非法状态
#define UFSR_INVPC       (1 << 18)   // 非法PC值
#define UFSR_NOCP        (1 << 19)   // 无协处理器
#define UFSR_UNALIGNED   (1 << 24)   // 未对齐访问
#define UFSR_DIVBYZERO   (1 << 25)   // 除零错误

static void print_fault_type(unsigned int cfsr)
{
    SHELL_LOG_SYS_ERROR("故障类型分析:");
    
    // Memory Management Fault 分析
    if (cfsr & CFSR_MMFSR_MASK) {
        SHELL_LOG_SYS_ERROR("Memory Management Fault detected:");
        if (cfsr & MMFSR_IACCVIOL) SHELL_LOG_SYS_ERROR("  - 指令访问违规");
        if (cfsr & MMFSR_DACCVIOL) SHELL_LOG_SYS_ERROR("  - 数据访问违规");
        if (cfsr & MMFSR_MUNSTKERR) SHELL_LOG_SYS_ERROR("  - 异常返回时栈操作错误");
        if (cfsr & MMFSR_MSTKERR) SHELL_LOG_SYS_ERROR("  - 异常入口时栈操作错误");
        if (cfsr & MMFSR_MLSPERR) SHELL_LOG_SYS_ERROR("  - 浮点单元延迟状态保存错误");
        if (cfsr & MMFSR_MMARVALID) SHELL_LOG_SYS_ERROR("  - MMAR 寄存器包含有效的故障地址");
    }
    
    // Bus Fault 分析
    if (cfsr & CFSR_BFSR_MASK) {
        SHELL_LOG_SYS_ERROR("Bus Fault detected:");
        if (cfsr & BFSR_IBUSERR) SHELL_LOG_SYS_ERROR("  - 指令总线错误");
        if (cfsr & BFSR_PRECISERR) SHELL_LOG_SYS_ERROR("  - 精确数据总线错误");
        if (cfsr & BFSR_IMPRECISERR) SHELL_LOG_SYS_ERROR("  - 非精确数据总线错误");
        if (cfsr & BFSR_UNSTKERR) SHELL_LOG_SYS_ERROR("  - 异常返回时栈操作错误");
        if (cfsr & BFSR_STKERR) SHELL_LOG_SYS_ERROR("  - 异常入口时栈操作错误");
        if (cfsr & BFSR_LSPERR) SHELL_LOG_SYS_ERROR("  - 浮点单元延迟状态保存错误");
        if (cfsr & BFSR_BFARVALID) SHELL_LOG_SYS_ERROR("  - BFAR 寄存器包含有效的故障地址");
    }
    
    // Usage Fault 分析
    if (cfsr & CFSR_UFSR_MASK) {
        SHELL_LOG_SYS_ERROR("Usage Fault detected:");
        if (cfsr & UFSR_UNDEFINSTR) SHELL_LOG_SYS_ERROR("  - 未定义指令");
        if (cfsr & UFSR_INVSTATE) SHELL_LOG_SYS_ERROR("  - 非法状态 (ARM/Thumb混合)");
        if (cfsr & UFSR_INVPC) SHELL_LOG_SYS_ERROR("  - 非法PC值");
        if (cfsr & UFSR_NOCP) SHELL_LOG_SYS_ERROR("  - 无协处理器");
        if (cfsr & UFSR_UNALIGNED) SHELL_LOG_SYS_ERROR("  - 未对齐访问");
        if (cfsr & UFSR_DIVBYZERO) SHELL_LOG_SYS_ERROR("  - 除零错误");
    }
}

void fault_handler_c(unsigned int *hardfault_args)
{
    unsigned int stacked_r0 = ((unsigned long)hardfault_args[0]);
    unsigned int stacked_r1 = ((unsigned long)hardfault_args[1]);
    unsigned int stacked_r2 = ((unsigned long)hardfault_args[2]);
    unsigned int stacked_r3 = ((unsigned long)hardfault_args[3]);
    unsigned int stacked_r12 = ((unsigned long)hardfault_args[4]);
    unsigned int stacked_lr = ((unsigned long)hardfault_args[5]);
    unsigned int stacked_pc = ((unsigned long)hardfault_args[6]);
    unsigned int stacked_psr = ((unsigned long)hardfault_args[7]);

    unsigned int cfsr = CFSR;
    unsigned int hfsr = HFSR;
    unsigned int dfsr = DFSR;
    unsigned int afsr = AFSR;
    unsigned int mmar = MMAR;
    unsigned int bfar = BFAR;

    shell_printf("\r\n");
    shell_printf("========================================\r\n");
    shell_printf("        MCU 异常故障信息转储           \r\n");
    shell_printf("========================================\r\n");
    
    // 打印 CPU 寄存器
    SHELL_LOG_SYS_ERROR("CPU 寄存器状态:");
    SHELL_LOG_SYS_ERROR("R0  = 0x%08X", stacked_r0);
    SHELL_LOG_SYS_ERROR("R1  = 0x%08X", stacked_r1);
    SHELL_LOG_SYS_ERROR("R2  = 0x%08X", stacked_r2);
    SHELL_LOG_SYS_ERROR("R3  = 0x%08X", stacked_r3);
    SHELL_LOG_SYS_ERROR("R12 = 0x%08X", stacked_r12);
    SHELL_LOG_SYS_ERROR("LR  = 0x%08X (异常前的链接寄存器)", stacked_lr);
    SHELL_LOG_SYS_ERROR("PC  = 0x%08X (故障发生地址)", stacked_pc);
    SHELL_LOG_SYS_ERROR("PSR = 0x%08X (程序状态寄存器)", stacked_psr);
    
    SHELL_LOG_SYS_ERROR("系统故障状态寄存器:");
    SHELL_LOG_SYS_ERROR("CFSR = 0x%08X (可配置故障状态寄存器)", cfsr);
    SHELL_LOG_SYS_ERROR("HFSR = 0x%08X (硬故障状态寄存器)", hfsr);
    SHELL_LOG_SYS_ERROR("DFSR = 0x%08X (调试故障状态寄存器)", dfsr);
    SHELL_LOG_SYS_ERROR("AFSR = 0x%08X (辅助故障状态寄存器)", afsr);
    
    if (cfsr & MMFSR_MMARVALID) {
        SHELL_LOG_SYS_ERROR("MMAR = 0x%08X (内存管理故障地址)", mmar);
    }
    if (cfsr & BFSR_BFARVALID) {
        SHELL_LOG_SYS_ERROR("BFAR = 0x%08X (总线故障地址)", bfar);
    }
    
    shell_printf("\r\n");
    print_fault_type(cfsr);

    // 简化的任务信息 - 避免在异常上下文中分配内存
    SHELL_LOG_SYS_ERROR("FreeRTOS 系统状态:");
    SHELL_LOG_SYS_ERROR("========================================");
    
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task != NULL) {
        SHELL_LOG_SYS_ERROR("当前任务: %s", pcTaskGetName(current_task));
    }
    
    SHELL_LOG_SYS_ERROR("任务数量: %u", (unsigned int)uxTaskGetNumberOfTasks());
    SHELL_LOG_SYS_ERROR("剩余堆空间: %u bytes", (unsigned int)xPortGetFreeHeapSize());
    
    SHELL_LOG_SYS_ERROR("========================================");
    SHELL_LOG_SYS_ERROR("系统已停止，请检查故障原因并重启");
    SHELL_LOG_SYS_ERROR("========================================");
    
    // 确保所有输出都被发送
    for(volatile int i = 0; i < 1000000; i++);
    
    while (1) {
        // 系统停止 - 避免重启循环
        __disable_irq();  // 禁用中断
        __WFE();          // 等待事件，降低功耗
    }
}
