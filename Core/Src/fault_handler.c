/**
  ******************************************************************************
  * @file           : fault_handler.c
  * @brief          : STM32H7 Fault Handler and Exception Dump Implementation
  * @description    : Comprehensive fault information capture and analysis
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025
  * All rights reserved.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "fault_handler.h"
#include "shell_log.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* FreeRTOS includes for task information */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* Private defines -----------------------------------------------------------*/
#define FAULT_PRINT_BUFFER_SIZE   256
#define STACK_DUMP_DEPTH          32
#define MEMORY_DUMP_WIDTH         16
#define MAX_BACKTRACE_DEPTH       20
#define MAX_TASKS_TO_DUMP         20

/* SCB register addresses for STM32H7 */
#define SCB_HFSR    (*(volatile uint32_t *)0xE000ED2C)  /* Hard Fault Status Register */
#define SCB_CFSR    (*(volatile uint32_t *)0xE000ED28)  /* Configurable Fault Status Register */
#define SCB_MMFAR   (*(volatile uint32_t *)0xE000ED34)  /* MemManage Fault Address Register */
#define SCB_BFAR    (*(volatile uint32_t *)0xE000ED38)  /* Bus Fault Address Register */
#define SCB_AFSR    (*(volatile uint32_t *)0xE000ED3C)  /* Auxiliary Fault Status Register */
#define SCB_DFSR    (*(volatile uint32_t *)0xE000ED30)  /* Debug Fault Status Register */

/* CFSR bit definitions */
/* MemManage Fault Status Register (MMFSR, bits [7:0] of CFSR) */
#define CFSR_MMARVALID    (1UL << 7)
#define CFSR_MLSPERR      (1UL << 5)
#define CFSR_MSTKERR      (1UL << 4)
#define CFSR_MUNSTKERR    (1UL << 3)
#define CFSR_DACCVIOL     (1UL << 1)
#define CFSR_IACCVIOL     (1UL << 0)

/* BusFault Status Register (BFSR, bits [15:8] of CFSR) */
#define CFSR_BFARVALID    (1UL << 15)
#define CFSR_LSPERR       (1UL << 13)
#define CFSR_STKERR       (1UL << 12)
#define CFSR_UNSTKERR     (1UL << 11)
#define CFSR_IMPRECISERR  (1UL << 10)
#define CFSR_PRECISERR    (1UL << 9)
#define CFSR_IBUSERR      (1UL << 8)

/* UsageFault Status Register (UFSR, bits [31:16] of CFSR) */
#define CFSR_DIVBYZERO    (1UL << 25)
#define CFSR_UNALIGNED    (1UL << 24)
#define CFSR_NOCP         (1UL << 19)
#define CFSR_INVPC        (1UL << 18)
#define CFSR_INVSTATE     (1UL << 17)
#define CFSR_UNDEFINSTR   (1UL << 16)

/* HFSR bit definitions */
#define HFSR_FORCED       (1UL << 30)
#define HFSR_VECTTBL      (1UL << 1)

/* Private variables ---------------------------------------------------------*/
volatile FaultInfo_t g_fault_info;
static char fault_print_buffer[FAULT_PRINT_BUFFER_SIZE];

/* Fault type strings */
static const char *fault_type_strings[] = {
    "HardFault",
    "MemManage Fault",
    "BusFault",
    "UsageFault",
    "NMI"
};

/* Private function prototypes -----------------------------------------------*/
static void FaultHandler_CollectContext(FaultContext_t *ctx, uint32_t *stack_ptr, uint32_t exc_return);
static void FaultHandler_CollectStatus(FaultStatus_t *status);
static void FaultHandler_PrintSeparator(void);
static void FaultHandler_PrintHeader(const char *title);
static uint8_t FaultHandler_IsValidMemory(uint32_t addr);
static uint8_t FaultHandler_IsValidCodeAddress(uint32_t addr);
static const char* FaultHandler_GetTaskStateString(eTaskState state);

/* Public functions ----------------------------------------------------------*/

/**
 * @brief Initialize fault handler module
 */
void FaultHandler_Init(void)
{
    /* Enable all configurable fault handlers */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk;
    
    /* Enable divide by zero and unaligned access traps */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    /* Uncomment the following line if you want to trap unaligned accesses */
    /* SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk; */
    
    /* Clear fault info structure */
    memset((void *)&g_fault_info, 0, sizeof(FaultInfo_t));
    
    /* Note: Don't use SHELL_LOG here as shell may not be initialized yet */
    /* Log message will be printed after shell_init() */
}

/**
 * @brief Low-level print function that works even in fault context
 */
void FaultHandler_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(fault_print_buffer, FAULT_PRINT_BUFFER_SIZE, format, args);
    va_end(args);
    
    /* Direct UART output - blocking mode, works even in fault context */
    HAL_UART_Transmit(&huart1, (uint8_t *)fault_print_buffer, 
                      strlen(fault_print_buffer), HAL_MAX_DELAY);
}

/**
 * @brief Get fault type string
 */
const char* FaultHandler_GetFaultTypeString(uint32_t fault_type)
{
    if (fault_type <= FAULT_TYPE_NMI) {
        return fault_type_strings[fault_type];
    }
    return "Unknown Fault";
}

/**
 * @brief Main fault handler processing
 */
void FaultHandler_Process(uint32_t *stack_ptr, uint32_t exc_return, uint32_t fault_type)
{
    FaultInfo_t *info = (FaultInfo_t *)&g_fault_info;
    
    /* Disable interrupts to prevent further issues */
    __disable_irq();
    
    /* Record basic fault information */
    info->fault_type = fault_type;
    info->exc_return = exc_return;
    info->active_stack = (exc_return & 0x04) ? 1 : 0;  /* PSP or MSP */
    info->timestamp = HAL_GetTick();
    
    /* Collect CPU context */
    FaultHandler_CollectContext(&info->context, stack_ptr, exc_return);
    
    /* Collect fault status registers */
    FaultHandler_CollectStatus(&info->status);
    
    /* Get current task name if FreeRTOS is running */
#if (configUSE_TRACE_FACILITY == 1)
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task != NULL) {
        strncpy(info->task_name, pcTaskGetName(current_task), sizeof(info->task_name) - 1);
        info->task_name[sizeof(info->task_name) - 1] = '\0';
    } else {
        strcpy(info->task_name, "N/A");
    }
#else
    strcpy(info->task_name, "N/A");
#endif
    
    /* Dump all fault information */
    FaultHandler_DumpInfo(info);
    
    /* Analyze and suggest possible causes */
    FaultHandler_AnalyzeFault(info);
    
    /* Dump possible backtrace */
    FaultHandler_DumpBacktrace(stack_ptr, info->context.pc, info->context.lr);
    
    /* Dump stack */
    FaultHandler_DumpStack(stack_ptr, STACK_DUMP_DEPTH);
    
    /* Dump all FreeRTOS tasks */
    FaultHandler_DumpTasks();
    
    /* Dump FreeRTOS timers */
    FaultHandler_DumpTimers();
    
    /* Clear fault status registers for next fault */
    SCB_HFSR = SCB_HFSR;  /* Write 1 to clear */
    SCB_CFSR = SCB_CFSR;
    
    /* Enter infinite loop */
    FaultHandler_Printf("\r\n" SHELL_COLOR_BRIGHT_RED 
                        "========== SYSTEM HALTED - RESET REQUIRED ==========\r\n"
                        SHELL_COLOR_RESET);
    
    while (1) {
        __NOP();
    }
}

/**
 * @brief Dump complete fault information
 */
void FaultHandler_DumpInfo(const FaultInfo_t *info)
{
    FaultHandler_PrintHeader("STM32H7 EXCEPTION DUMP");
    
    /* Basic fault info */
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                        ">>> %s OCCURRED! <<<\r\n" 
                        SHELL_COLOR_RESET, 
                        FaultHandler_GetFaultTypeString(info->fault_type));
    
    FaultHandler_Printf(SHELL_COLOR_CYAN "Timestamp:    " SHELL_COLOR_RESET "%lu ms\r\n", 
                        info->timestamp);
    FaultHandler_Printf(SHELL_COLOR_CYAN "Task Name:    " SHELL_COLOR_RESET "%s\r\n", 
                        info->task_name);
    FaultHandler_Printf(SHELL_COLOR_CYAN "Active Stack: " SHELL_COLOR_RESET "%s (EXC_RETURN=0x%08lX)\r\n",
                        info->active_stack ? "PSP (Process)" : "MSP (Main)", 
                        info->exc_return);
    
    /* CPU Registers */
    FaultHandler_PrintHeader("CPU REGISTERS");
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "=== Stack Frame Registers ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("R0  = 0x%08lX    R1  = 0x%08lX\r\n", 
                        info->context.r0, info->context.r1);
    FaultHandler_Printf("R2  = 0x%08lX    R3  = 0x%08lX\r\n", 
                        info->context.r2, info->context.r3);
    FaultHandler_Printf("R12 = 0x%08lX\r\n", info->context.r12);
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_YELLOW 
                        "LR  = 0x%08lX    (Return Address)\r\n" SHELL_COLOR_RESET, 
                        info->context.lr);
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                        "PC  = 0x%08lX    (Fault Address)\r\n" SHELL_COLOR_RESET, 
                        info->context.pc);
    FaultHandler_Printf("xPSR= 0x%08lX\r\n", info->context.xpsr);
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Saved Registers ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("R4  = 0x%08lX    R5  = 0x%08lX\r\n", 
                        info->context.r4, info->context.r5);
    FaultHandler_Printf("R6  = 0x%08lX    R7  = 0x%08lX\r\n", 
                        info->context.r6, info->context.r7);
    FaultHandler_Printf("R8  = 0x%08lX    R9  = 0x%08lX\r\n", 
                        info->context.r8, info->context.r9);
    FaultHandler_Printf("R10 = 0x%08lX    R11 = 0x%08lX\r\n", 
                        info->context.r10, info->context.r11);
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Stack Pointers ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("MSP = 0x%08lX    PSP = 0x%08lX\r\n", 
                        info->context.msp, info->context.psp);
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Special Registers ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("CONTROL  = 0x%08lX\r\n", info->context.control);
    FaultHandler_Printf("BASEPRI  = 0x%08lX\r\n", info->context.basepri);
    FaultHandler_Printf("PRIMASK  = 0x%08lX\r\n", info->context.primask);
    FaultHandler_Printf("FAULTMASK= 0x%08lX\r\n", info->context.faultmask);
    FaultHandler_Printf("FPSCR    = 0x%08lX\r\n", info->context.fpscr);
    
    /* Fault Status Registers */
    FaultHandler_PrintHeader("FAULT STATUS REGISTERS");
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "=== Hard Fault Status (HFSR) ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("HFSR     = 0x%08lX\r\n", info->status.hfsr);
    if (info->status.hfsr_forced) {
        FaultHandler_Printf(SHELL_COLOR_RED "  [FORCED]   Escalated from configurable fault\r\n" SHELL_COLOR_RESET);
    }
    if (info->status.hfsr_vecttbl) {
        FaultHandler_Printf(SHELL_COLOR_RED "  [VECTTBL]  Vector table read fault\r\n" SHELL_COLOR_RESET);
    }
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Configurable Fault Status (CFSR) ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("CFSR     = 0x%08lX\r\n", info->status.cfsr);
    
    /* MemManage Fault */
    FaultHandler_Printf(SHELL_COLOR_MAGENTA "\r\n--- MemManage Fault Status (MMFSR) ---\r\n" SHELL_COLOR_RESET);
    if (info->status.mmfsr_iaccviol)
        FaultHandler_Printf(SHELL_COLOR_RED "  [IACCVIOL]  Instruction access violation\r\n" SHELL_COLOR_RESET);
    if (info->status.mmfsr_daccviol)
        FaultHandler_Printf(SHELL_COLOR_RED "  [DACCVIOL]  Data access violation\r\n" SHELL_COLOR_RESET);
    if (info->status.mmfsr_munstkerr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [MUNSTKERR] Unstacking error\r\n" SHELL_COLOR_RESET);
    if (info->status.mmfsr_mstkerr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [MSTKERR]   Stacking error\r\n" SHELL_COLOR_RESET);
    if (info->status.mmfsr_mmarvalid) {
        FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED "  [MMARVALID] MMFAR = 0x%08lX (Fault Address)\r\n" SHELL_COLOR_RESET, 
                            info->status.mmfar);
    }
    
    /* BusFault */
    FaultHandler_Printf(SHELL_COLOR_MAGENTA "\r\n--- Bus Fault Status (BFSR) ---\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_ibuserr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [IBUSERR]   Instruction bus error\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_preciserr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [PRECISERR] Precise data bus error\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_impreciserr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [IMPRECISERR] Imprecise data bus error\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_unstkerr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [UNSTKERR]  Unstacking error\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_stkerr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [STKERR]    Stacking error\r\n" SHELL_COLOR_RESET);
    if (info->status.bfsr_bfarvalid) {
        FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED "  [BFARVALID] BFAR = 0x%08lX (Fault Address)\r\n" SHELL_COLOR_RESET, 
                            info->status.bfar);
    }
    
    /* UsageFault */
    FaultHandler_Printf(SHELL_COLOR_MAGENTA "\r\n--- Usage Fault Status (UFSR) ---\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_undefinstr)
        FaultHandler_Printf(SHELL_COLOR_RED "  [UNDEFINSTR] Undefined instruction\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_invstate)
        FaultHandler_Printf(SHELL_COLOR_RED "  [INVSTATE]   Invalid EPSR.T bit or illegal EPSR.IT\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_invpc)
        FaultHandler_Printf(SHELL_COLOR_RED "  [INVPC]      Invalid PC load (bad EXC_RETURN)\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_nocp)
        FaultHandler_Printf(SHELL_COLOR_RED "  [NOCP]       No coprocessor (FPU not enabled?)\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_unaligned)
        FaultHandler_Printf(SHELL_COLOR_RED "  [UNALIGNED]  Unaligned memory access\r\n" SHELL_COLOR_RESET);
    if (info->status.ufsr_divbyzero)
        FaultHandler_Printf(SHELL_COLOR_RED "  [DIVBYZERO]  Divide by zero\r\n" SHELL_COLOR_RESET);
    
    /* Other registers */
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Other Status Registers ===\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("AFSR     = 0x%08lX\r\n", info->status.afsr);
    FaultHandler_Printf("DFSR     = 0x%08lX\r\n", info->status.dfsr);
}

/**
 * @brief Analyze fault and provide possible causes
 */
void FaultHandler_AnalyzeFault(const FaultInfo_t *info)
{
    FaultHandler_PrintHeader("FAULT ANALYSIS");
    
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_WHITE "Possible causes:\r\n" SHELL_COLOR_RESET);
    
    /* Analyze based on fault type and status bits */
    if (info->status.ufsr_divbyzero) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Division by zero detected\r\n"
            "    Check code near PC=0x%08lX for divide operations\r\n" 
            SHELL_COLOR_RESET, info->context.pc);
    }
    
    if (info->status.ufsr_undefinstr) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Undefined instruction at PC=0x%08lX\r\n"
            "    Possible causes:\r\n"
            "      * Corrupted code memory\r\n"
            "      * Jump to invalid address\r\n"
            "      * Stack overflow corrupting return address\r\n"
            SHELL_COLOR_RESET, info->context.pc);
    }
    
    if (info->status.ufsr_invstate) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Invalid execution state (Thumb bit issue)\r\n"
            "    Possible causes:\r\n"
            "      * BX/BLX to address without LSB set\r\n"
            "      * Function pointer without +1 offset\r\n"
            "      * Corrupted return address on stack\r\n"
            SHELL_COLOR_RESET);
    }
    
    if (info->status.ufsr_invpc) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Invalid PC load during exception return\r\n"
            "    Possible causes:\r\n"
            "      * Corrupted stack\r\n"
            "      * Invalid EXC_RETURN value\r\n"
            SHELL_COLOR_RESET);
    }
    
    if (info->status.ufsr_nocp) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Coprocessor (FPU) access error\r\n"
            "    FPU might not be enabled or incorrect access\r\n"
            SHELL_COLOR_RESET);
    }
    
    if (info->status.mmfsr_iaccviol || info->status.mmfsr_daccviol) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Memory protection violation\r\n"
            "    Address: 0x%08lX\r\n"
            "    Possible causes:\r\n"
            "      * Access to non-existent memory\r\n"
            "      * MPU region violation\r\n"
            "      * NULL pointer dereference\r\n"
            SHELL_COLOR_RESET, info->status.mmfar);
    }
    
    if (info->status.mmfsr_mstkerr || info->status.bfsr_stkerr) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Stack overflow during exception entry\r\n"
            "    Check stack sizes and usage\r\n"
            SHELL_COLOR_RESET);
    }
    
    if (info->status.bfsr_preciserr) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Precise bus error at address 0x%08lX\r\n"
            "    Possible causes:\r\n"
            "      * Access to invalid peripheral address\r\n"
            "      * Peripheral clock not enabled\r\n"
            "      * Memory not initialized\r\n"
            SHELL_COLOR_RESET, info->status.bfar);
    }
    
    if (info->status.bfsr_impreciserr) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Imprecise bus error (write buffer)\r\n"
            "    The actual fault occurred before PC=0x%08lX\r\n"
            "    Hard to pinpoint exact location\r\n"
            SHELL_COLOR_RESET, info->context.pc);
    }
    
    if (info->status.bfsr_ibuserr) {
        FaultHandler_Printf(SHELL_COLOR_YELLOW 
            "  - Instruction fetch bus error\r\n"
            "    Jump to invalid code region\r\n"
            SHELL_COLOR_RESET);
    }
    
    /* General suggestions */
    FaultHandler_Printf(SHELL_COLOR_CYAN 
        "\r\nDebugging tips:\r\n"
        "  1. Check LR (0x%08lX) for caller function\r\n"
        "  2. Examine code around PC (0x%08lX)\r\n"
        "  3. Review stack dump for corruption\r\n"
        "  4. Use addr2line or objdump to find source location:\r\n"
        "     arm-none-eabi-addr2line -e firmware.elf 0x%08lX\r\n"
        SHELL_COLOR_RESET, 
        info->context.lr, info->context.pc, info->context.pc);
}

/**
 * @brief Dump stack memory
 */
void FaultHandler_DumpStack(uint32_t *stack_ptr, uint32_t depth)
{
    FaultHandler_PrintHeader("STACK DUMP");
    
    FaultHandler_Printf(SHELL_COLOR_CYAN "Stack Pointer: 0x%08lX\r\n\r\n" SHELL_COLOR_RESET, 
                        (uint32_t)stack_ptr);
    
    FaultHandler_Printf("Offset    Address      Value\r\n");
    FaultHandler_Printf("------    --------     --------\r\n");
    
    for (uint32_t i = 0; i < depth; i++) {
        uint32_t addr = (uint32_t)(stack_ptr + i);
        
        /* Check if address is valid */
        if (!FaultHandler_IsValidMemory(addr)) {
            FaultHandler_Printf("[%2lu]      0x%08lX   <invalid>\r\n", i * 4, addr);
            break;
        }
        
        uint32_t value = *(stack_ptr + i);
        
        /* Highlight special stack frame positions */
        const char *marker = "";
        if (i == STACK_FRAME_R0) marker = " <- R0";
        else if (i == STACK_FRAME_R1) marker = " <- R1";
        else if (i == STACK_FRAME_R2) marker = " <- R2";
        else if (i == STACK_FRAME_R3) marker = " <- R3";
        else if (i == STACK_FRAME_R12) marker = " <- R12";
        else if (i == STACK_FRAME_LR) marker = " <- LR (Return Addr)";
        else if (i == STACK_FRAME_PC) marker = " <- PC (Fault Addr)";
        else if (i == STACK_FRAME_XPSR) marker = " <- xPSR";
        
        if (strlen(marker) > 0) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_YELLOW 
                "[%2lu]      0x%08lX   0x%08lX%s\r\n" SHELL_COLOR_RESET, 
                i * 4, addr, value, marker);
        } else {
            FaultHandler_Printf("[%2lu]      0x%08lX   0x%08lX\r\n", 
                i * 4, addr, value);
        }
    }
}

/**
 * @brief Dump memory region
 */
void FaultHandler_DumpMemory(uint32_t addr, uint32_t length)
{
    FaultHandler_PrintHeader("MEMORY DUMP");
    
    /* Align to 16-byte boundary */
    uint32_t start_addr = addr & ~0x0F;
    uint32_t end_addr = (addr + length + 15) & ~0x0F;
    
    FaultHandler_Printf("Address    ");
    for (int i = 0; i < 16; i++) {
        FaultHandler_Printf("%02X ", i);
    }
    FaultHandler_Printf(" ASCII\r\n");
    
    FaultHandler_Printf("---------  ");
    for (int i = 0; i < 16; i++) {
        FaultHandler_Printf("-- ");
    }
    FaultHandler_Printf(" ----------------\r\n");
    
    for (uint32_t a = start_addr; a < end_addr; a += 16) {
        if (!FaultHandler_IsValidMemory(a)) {
            FaultHandler_Printf("0x%08lX <invalid memory region>\r\n", a);
            continue;
        }
        
        FaultHandler_Printf("0x%08lX ", a);
        
        /* Hex dump */
        char ascii[17];
        for (int i = 0; i < 16; i++) {
            uint8_t byte = *((uint8_t *)(a + i));
            FaultHandler_Printf("%02X ", byte);
            ascii[i] = (byte >= 0x20 && byte < 0x7F) ? byte : '.';
        }
        ascii[16] = '\0';
        
        FaultHandler_Printf(" %s\r\n", ascii);
    }
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Collect CPU context from stack and registers
 */
static void FaultHandler_CollectContext(FaultContext_t *ctx, uint32_t *stack_ptr, uint32_t exc_return)
{
    /* Get registers from exception stack frame */
    ctx->r0   = stack_ptr[STACK_FRAME_R0];
    ctx->r1   = stack_ptr[STACK_FRAME_R1];
    ctx->r2   = stack_ptr[STACK_FRAME_R2];
    ctx->r3   = stack_ptr[STACK_FRAME_R3];
    ctx->r12  = stack_ptr[STACK_FRAME_R12];
    ctx->lr   = stack_ptr[STACK_FRAME_LR];
    ctx->pc   = stack_ptr[STACK_FRAME_PC];
    ctx->xpsr = stack_ptr[STACK_FRAME_XPSR];
    
    /* Get other registers using inline assembly */
    __asm volatile (
        "MOV %0, R4\n"
        "MOV %1, R5\n"
        "MOV %2, R6\n"
        "MOV %3, R7\n"
        : "=r"(ctx->r4), "=r"(ctx->r5), "=r"(ctx->r6), "=r"(ctx->r7)
    );
    
    __asm volatile (
        "MOV %0, R8\n"
        "MOV %1, R9\n"
        "MOV %2, R10\n"
        "MOV %3, R11\n"
        : "=r"(ctx->r8), "=r"(ctx->r9), "=r"(ctx->r10), "=r"(ctx->r11)
    );
    
    /* Get stack pointers */
    ctx->msp = __get_MSP();
    ctx->psp = __get_PSP();
    
    /* Get special registers */
    ctx->control = __get_CONTROL();
    ctx->basepri = __get_BASEPRI();
    ctx->primask = __get_PRIMASK();
    ctx->faultmask = __get_FAULTMASK();
    
    /* Get FPU status if FPU is enabled */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    ctx->fpscr = __get_FPSCR();
#else
    ctx->fpscr = 0;
#endif
}

/**
 * @brief Collect fault status registers
 */
static void FaultHandler_CollectStatus(FaultStatus_t *status)
{
    /* Read fault status registers */
    status->hfsr = SCB_HFSR;
    status->cfsr = SCB_CFSR;
    status->mmfar = SCB_MMFAR;
    status->bfar = SCB_BFAR;
    status->afsr = SCB_AFSR;
    status->dfsr = SCB_DFSR;
    
    /* Parse HFSR */
    status->hfsr_forced = (status->hfsr & HFSR_FORCED) ? 1 : 0;
    status->hfsr_vecttbl = (status->hfsr & HFSR_VECTTBL) ? 1 : 0;
    
    /* Parse MMFSR (bits 7:0 of CFSR) */
    status->mmfsr_mmarvalid = (status->cfsr & CFSR_MMARVALID) ? 1 : 0;
    status->mmfsr_mstkerr = (status->cfsr & CFSR_MSTKERR) ? 1 : 0;
    status->mmfsr_munstkerr = (status->cfsr & CFSR_MUNSTKERR) ? 1 : 0;
    status->mmfsr_daccviol = (status->cfsr & CFSR_DACCVIOL) ? 1 : 0;
    status->mmfsr_iaccviol = (status->cfsr & CFSR_IACCVIOL) ? 1 : 0;
    
    /* Parse BFSR (bits 15:8 of CFSR) */
    status->bfsr_bfarvalid = (status->cfsr & CFSR_BFARVALID) ? 1 : 0;
    status->bfsr_stkerr = (status->cfsr & CFSR_STKERR) ? 1 : 0;
    status->bfsr_unstkerr = (status->cfsr & CFSR_UNSTKERR) ? 1 : 0;
    status->bfsr_impreciserr = (status->cfsr & CFSR_IMPRECISERR) ? 1 : 0;
    status->bfsr_preciserr = (status->cfsr & CFSR_PRECISERR) ? 1 : 0;
    status->bfsr_ibuserr = (status->cfsr & CFSR_IBUSERR) ? 1 : 0;
    
    /* Parse UFSR (bits 31:16 of CFSR) */
    status->ufsr_divbyzero = (status->cfsr & CFSR_DIVBYZERO) ? 1 : 0;
    status->ufsr_unaligned = (status->cfsr & CFSR_UNALIGNED) ? 1 : 0;
    status->ufsr_nocp = (status->cfsr & CFSR_NOCP) ? 1 : 0;
    status->ufsr_invpc = (status->cfsr & CFSR_INVPC) ? 1 : 0;
    status->ufsr_invstate = (status->cfsr & CFSR_INVSTATE) ? 1 : 0;
    status->ufsr_undefinstr = (status->cfsr & CFSR_UNDEFINSTR) ? 1 : 0;
}

/**
 * @brief Print separator line
 */
static void FaultHandler_PrintSeparator(void)
{
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_BLACK 
                        "================================================================\r\n"
                        SHELL_COLOR_RESET);
}

/**
 * @brief Print section header
 */
static void FaultHandler_PrintHeader(const char *title)
{
    FaultHandler_Printf("\r\n");
    FaultHandler_PrintSeparator();
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_WHITE SHELL_STYLE_BOLD 
                        " %s\r\n" 
                        SHELL_COLOR_RESET, title);
    FaultHandler_PrintSeparator();
}

/**
 * @brief Check if memory address is valid for reading
 */
static uint8_t FaultHandler_IsValidMemory(uint32_t addr)
{
    /* STM32H750 memory map ranges */
    /* ITCM RAM: 0x00000000 - 0x0000FFFF (64KB) */
    if (addr >= 0x00000000 && addr < 0x00010000) return 1;
    
    /* Flash: 0x08000000 - 0x0801FFFF (128KB for H750) */
    if (addr >= 0x08000000 && addr < 0x08020000) return 1;
    
    /* DTCM RAM: 0x20000000 - 0x2001FFFF (128KB) */
    if (addr >= 0x20000000 && addr < 0x20020000) return 1;
    
    /* AXI SRAM (D1): 0x24000000 - 0x2407FFFF (512KB) */
    if (addr >= 0x24000000 && addr < 0x24080000) return 1;
    
    /* SRAM1 (D2): 0x30000000 - 0x3001FFFF (128KB) */
    if (addr >= 0x30000000 && addr < 0x30020000) return 1;
    
    /* SRAM2 (D2): 0x30020000 - 0x3003FFFF (128KB) */
    if (addr >= 0x30020000 && addr < 0x30040000) return 1;
    
    /* SRAM3 (D2): 0x30040000 - 0x30047FFF (32KB) */
    if (addr >= 0x30040000 && addr < 0x30048000) return 1;
    
    /* SRAM4 (D3): 0x38000000 - 0x3800FFFF (64KB) */
    if (addr >= 0x38000000 && addr < 0x38010000) return 1;
    
    /* Backup SRAM: 0x38800000 - 0x38800FFF (4KB) */
    if (addr >= 0x38800000 && addr < 0x38801000) return 1;
    
    /* External QSPI Flash: 0x90000000 - 0x9FFFFFFF */
    if (addr >= 0x90000000 && addr < 0xA0000000) return 1;
    
    /* External SDRAM (FMC): 0xC0000000 - 0xCFFFFFFF */
    if (addr >= 0xC0000000 && addr < 0xD0000000) return 1;
    
    /* Peripheral region: 0x40000000 - 0x5FFFFFFF */
    if (addr >= 0x40000000 && addr < 0x60000000) return 1;
    
    /* Core peripherals: 0xE0000000 - 0xE00FFFFF */
    if (addr >= 0xE0000000 && addr < 0xE0100000) return 1;
    
    return 0;
}

/* C handler functions called from assembly wrappers */

void HardFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return)
{
    FaultHandler_Process(stack_ptr, exc_return, FAULT_TYPE_HARDFAULT);
}

void MemManage_Handler_C(uint32_t *stack_ptr, uint32_t exc_return)
{
    FaultHandler_Process(stack_ptr, exc_return, FAULT_TYPE_MEMMANAGE);
}

void BusFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return)
{
    FaultHandler_Process(stack_ptr, exc_return, FAULT_TYPE_BUSFAULT);
}

void UsageFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return)
{
    FaultHandler_Process(stack_ptr, exc_return, FAULT_TYPE_USAGEFAULT);
}

/**
 * @brief Get task state string
 */
static const char* FaultHandler_GetTaskStateString(eTaskState state)
{
    switch (state) {
        case eRunning:   return "RUNNING";
        case eReady:     return "READY";
        case eBlocked:   return "BLOCKED";
        case eSuspended: return "SUSPEND";
        case eDeleted:   return "DELETED";
        default:         return "UNKNOWN";
    }
}

/**
 * @brief Check if address is a valid code address
 */
static uint8_t FaultHandler_IsValidCodeAddress(uint32_t addr)
{
    /* Flash: 0x08000000 - 0x0801FFFF (128KB for H750) */
    if (addr >= 0x08000000 && addr < 0x08020000) return 1;
    
    /* External QSPI Flash: 0x90000000 - 0x9FFFFFFF */
    if (addr >= 0x90000000 && addr < 0xA0000000) return 1;
    
    /* ITCM RAM (can contain code): 0x00000000 - 0x0000FFFF (64KB) */
    if (addr >= 0x00000000 && addr < 0x00010000) return 1;
    
    /* DTCM/AXI SRAM (can contain code in some cases) */
    if (addr >= 0x20000000 && addr < 0x24080000) return 1;
    
    return 0;
}

/**
 * @brief Dump possible call stack backtrace
 */
void FaultHandler_DumpBacktrace(uint32_t *stack_ptr, uint32_t pc, uint32_t lr)
{
    FaultHandler_PrintHeader("POSSIBLE BACKTRACE");
    
    FaultHandler_Printf(SHELL_COLOR_CYAN 
        "Note: This is a heuristic backtrace. Use with caution.\r\n"
        "      For accurate trace, use debugger or addr2line tool.\r\n\r\n"
        SHELL_COLOR_RESET);
    
    uint32_t backtrace[MAX_BACKTRACE_DEPTH];
    int bt_count = 0;
    
    /* First entry is always the fault PC */
    backtrace[bt_count++] = pc;
    
    /* Second entry is the LR (caller before fault) */
    if (FaultHandler_IsValidCodeAddress(lr & ~1)) {
        backtrace[bt_count++] = lr & ~1;  /* Clear Thumb bit */
    }
    
    /* Scan stack for potential return addresses */
    /* In ARM Cortex-M, return addresses have bit 0 set (Thumb mode) */
    /* and should point to valid code regions */
    for (int i = 0; i < STACK_DUMP_DEPTH && bt_count < MAX_BACKTRACE_DEPTH; i++) {
        uint32_t addr = (uint32_t)(stack_ptr + i);
        
        if (!FaultHandler_IsValidMemory(addr)) {
            break;
        }
        
        uint32_t value = *(stack_ptr + i);
        
        /* Check if this looks like a return address */
        /* Return addresses in Thumb mode have LSB set */
        if ((value & 1) && FaultHandler_IsValidCodeAddress(value & ~1)) {
            /* Avoid duplicates */
            int is_duplicate = 0;
            for (int j = 0; j < bt_count; j++) {
                if (backtrace[j] == (value & ~1)) {
                    is_duplicate = 1;
                    break;
                }
            }
            if (!is_duplicate) {
                backtrace[bt_count++] = value & ~1;
            }
        }
    }
    
    /* Print backtrace */
    FaultHandler_Printf(SHELL_COLOR_YELLOW "Call Stack (most recent first):\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("  # | Address    | Info\r\n");
    FaultHandler_Printf("----+------------+------------------------------------------\r\n");
    
    for (int i = 0; i < bt_count; i++) {
        const char *location = "";
        
        /* Try to identify memory region */
        if (backtrace[i] >= 0x08000000 && backtrace[i] < 0x08020000) {
            location = "(Internal Flash)";
        } else if (backtrace[i] >= 0x90000000 && backtrace[i] < 0xA0000000) {
            location = "(External QSPI Flash - App)";
        } else if (backtrace[i] >= 0x20000000 && backtrace[i] < 0x24080000) {
            location = "(RAM - possibly loaded code)";
        }
        
        if (i == 0) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                " %2d | 0x%08lX | << FAULT PC >> %s\r\n" 
                SHELL_COLOR_RESET, i, backtrace[i], location);
        } else if (i == 1 && FaultHandler_IsValidCodeAddress(lr & ~1)) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_YELLOW 
                " %2d | 0x%08lX | << Caller (LR) >> %s\r\n" 
                SHELL_COLOR_RESET, i, backtrace[i], location);
        } else {
            FaultHandler_Printf(" %2d | 0x%08lX | %s\r\n", i, backtrace[i], location);
        }
    }
    
    /* Print addr2line commands */
    FaultHandler_Printf(SHELL_COLOR_CYAN 
        "\r\nTo decode addresses, run:\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("  arm-none-eabi-addr2line -e firmware.elf -f -p");
    for (int i = 0; i < bt_count && i < 5; i++) {
        FaultHandler_Printf(" 0x%08lX", backtrace[i]);
    }
    FaultHandler_Printf("\r\n");
}

/**
 * @brief Dump all FreeRTOS task information
 */
void FaultHandler_DumpTasks(void)
{
    FaultHandler_PrintHeader("FREERTOS TASKS");
    
#if (configUSE_TRACE_FACILITY == 1)
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    
    FaultHandler_Printf(SHELL_COLOR_CYAN "Total Tasks: %lu\r\n\r\n" SHELL_COLOR_RESET, 
                        (unsigned long)num_tasks);
    
    /* Get current task */
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    const char *current_task_name = (current_task != NULL) ? pcTaskGetName(current_task) : "N/A";
    
    FaultHandler_Printf(SHELL_COLOR_BRIGHT_YELLOW ">>> Current/Faulting Task: %s <<<\r\n\r\n" 
                        SHELL_COLOR_RESET, current_task_name);
    
#if (configUSE_STATS_FORMATTING_FUNCTIONS == 1) && (configGENERATE_RUN_TIME_STATS == 1)
    /* Use vTaskList if available */
    FaultHandler_Printf("Name            State  Pri  Stack   Num\r\n");
    FaultHandler_Printf("--------------- -----  ---  ------  ---\r\n");
    
    /* Allocate buffer for task list */
    static char task_list_buffer[512];
    vTaskList(task_list_buffer);
    
    /* Print line by line, highlighting current task */
    char *line = task_list_buffer;
    while (*line) {
        char *next_line = strchr(line, '\n');
        if (next_line) *next_line = '\0';
        
        /* Check if this is the current task */
        if (current_task_name && strstr(line, current_task_name) == line) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED "%s\r\n" SHELL_COLOR_RESET, line);
        } else {
            FaultHandler_Printf("%s\r\n", line);
        }
        
        if (next_line) {
            line = next_line + 1;
        } else {
            break;
        }
    }
#else
    /* Manual task enumeration if vTaskList not available */
    TaskStatus_t task_status[MAX_TASKS_TO_DUMP];
    uint32_t total_runtime;
    
    UBaseType_t tasks_returned = uxTaskGetSystemState(task_status, MAX_TASKS_TO_DUMP, &total_runtime);
    
    FaultHandler_Printf("Name            State    Pri  StackHWM  StackBase   TaskNum\r\n");
    FaultHandler_Printf("--------------- -------- ---  --------  ----------  -------\r\n");
    
    for (UBaseType_t i = 0; i < tasks_returned; i++) {
        TaskStatus_t *ts = &task_status[i];
        const char *state_str = FaultHandler_GetTaskStateString(ts->eCurrentState);
        
        /* Highlight current task */
        if (ts->xHandle == current_task) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                "%-15s %-8s %3lu  %8lu  0x%08lX  %7lu << FAULT\r\n" 
                SHELL_COLOR_RESET,
                ts->pcTaskName,
                state_str,
                (unsigned long)ts->uxCurrentPriority,
                (unsigned long)ts->usStackHighWaterMark,
                (unsigned long)ts->pxStackBase,
                (unsigned long)ts->xTaskNumber);
        } else {
            FaultHandler_Printf("%-15s %-8s %3lu  %8lu  0x%08lX  %7lu\r\n",
                ts->pcTaskName,
                state_str,
                (unsigned long)ts->uxCurrentPriority,
                (unsigned long)ts->usStackHighWaterMark,
                (unsigned long)ts->pxStackBase,
                (unsigned long)ts->xTaskNumber);
        }
    }
#endif
    
    /* Print stack high water mark warnings */
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\n=== Stack Usage Analysis ===\r\n" SHELL_COLOR_RESET);
    
    TaskStatus_t task_status2[MAX_TASKS_TO_DUMP];
    uint32_t total_runtime2;
    UBaseType_t tasks_count = uxTaskGetSystemState(task_status2, MAX_TASKS_TO_DUMP, &total_runtime2);
    
    for (UBaseType_t i = 0; i < tasks_count; i++) {
        TaskStatus_t *ts = &task_status2[i];
        UBaseType_t hwm = ts->usStackHighWaterMark;
        
        /* Warning if high water mark is low (less than 50 words = 200 bytes) */
        if (hwm < 50) {
            FaultHandler_Printf(SHELL_COLOR_RED 
                "WARNING: Task '%s' stack nearly full! HWM=%lu words\r\n" 
                SHELL_COLOR_RESET, ts->pcTaskName, (unsigned long)hwm);
        } else if (hwm < 100) {
            FaultHandler_Printf(SHELL_COLOR_YELLOW 
                "CAUTION: Task '%s' stack usage high. HWM=%lu words\r\n" 
                SHELL_COLOR_RESET, ts->pcTaskName, (unsigned long)hwm);
        }
        
        /* Highlight if this is the faulting task */
        if (ts->xHandle == current_task) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                ">> Faulting task '%s': StackBase=0x%08lX, HWM=%lu words\r\n"
                SHELL_COLOR_RESET,
                ts->pcTaskName,
                (unsigned long)ts->pxStackBase,
                (unsigned long)hwm);
        }
    }
    
#else
    FaultHandler_Printf("Task tracing not enabled (configUSE_TRACE_FACILITY=0)\r\n");
#endif
}

/**
 * @brief Dump FreeRTOS software timer information
 */
void FaultHandler_DumpTimers(void)
{
    FaultHandler_PrintHeader("FREERTOS TIMERS");
    
#if (configUSE_TIMERS == 1)
    FaultHandler_Printf(SHELL_COLOR_CYAN 
        "Timer Task Configuration:\r\n" SHELL_COLOR_RESET);
    FaultHandler_Printf("  Priority:    %d\r\n", configTIMER_TASK_PRIORITY);
    FaultHandler_Printf("  Stack Depth: %d words\r\n", configTIMER_TASK_STACK_DEPTH);
    FaultHandler_Printf("  Queue Len:   %d\r\n", configTIMER_QUEUE_LENGTH);
    
    /* Get timer task handle */
    TaskHandle_t timer_task = xTimerGetTimerDaemonTaskHandle();
    if (timer_task != NULL) {
        UBaseType_t timer_hwm = uxTaskGetStackHighWaterMark(timer_task);
        FaultHandler_Printf("  Stack HWM:   %lu words\r\n", (unsigned long)timer_hwm);
        
        if (timer_hwm < 50) {
            FaultHandler_Printf(SHELL_COLOR_RED 
                "  WARNING: Timer task stack nearly full!\r\n" SHELL_COLOR_RESET);
        }
        
        /* Check if timer task is current task */
        TaskHandle_t current = xTaskGetCurrentTaskHandle();
        if (timer_task == current) {
            FaultHandler_Printf(SHELL_COLOR_BRIGHT_RED 
                "\r\n>>> FAULT OCCURRED IN TIMER TASK! <<<\r\n"
                "    Check timer callback functions for issues.\r\n"
                SHELL_COLOR_RESET);
        }
    }
    
    FaultHandler_Printf(SHELL_COLOR_YELLOW 
        "\r\nNote: Individual timer details require custom tracking.\r\n"
        "      Check timer callback functions if Timer Task faulted.\r\n"
        SHELL_COLOR_RESET);
    
    /* Provide debugging hints for timer issues */
    FaultHandler_Printf(SHELL_COLOR_CYAN 
        "\r\nTimer debugging tips:\r\n"
        "  1. Timer callbacks run in Timer Task context\r\n"
        "  2. Long callbacks can delay other timers\r\n"
        "  3. Do NOT call blocking APIs in timer callbacks\r\n"
        "  4. Check for stack overflow in timer callbacks\r\n"
        SHELL_COLOR_RESET);
    
#else
    FaultHandler_Printf("Software timers not enabled (configUSE_TIMERS=0)\r\n");
#endif
}

/**
 * @brief Dump specific task's stack usage
 */
void FaultHandler_DumpTaskStack(void *task_handle)
{
    FaultHandler_PrintHeader("TASK STACK DETAILS");
    
#if (configUSE_TRACE_FACILITY == 1)
    TaskHandle_t task = (TaskHandle_t)task_handle;
    if (task == NULL) {
        task = xTaskGetCurrentTaskHandle();
    }
    
    if (task == NULL) {
        FaultHandler_Printf("No task available\r\n");
        return;
    }
    
    const char *name = pcTaskGetName(task);
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(task);
    
    FaultHandler_Printf("Task: %s\r\n", name);
    FaultHandler_Printf("Stack High Water Mark: %lu words (%lu bytes)\r\n", 
                        (unsigned long)hwm, (unsigned long)(hwm * 4));
    
    /* Get task status for more details */
    TaskStatus_t status;
    vTaskGetInfo(task, &status, pdTRUE, eInvalid);
    
    FaultHandler_Printf("Stack Base: 0x%08lX\r\n", (unsigned long)status.pxStackBase);
    FaultHandler_Printf("Task Number: %lu\r\n", (unsigned long)status.xTaskNumber);
    FaultHandler_Printf("Min Free Ever: %lu words (%lu bytes)\r\n", 
                        (unsigned long)hwm, (unsigned long)(hwm * 4));
    
    /* Dump stack from base */
    FaultHandler_Printf(SHELL_COLOR_YELLOW "\r\nStack Base Content:\r\n" SHELL_COLOR_RESET);
    uint32_t *sp = (uint32_t *)status.pxStackBase;
    for (int i = 0; i < 16 && FaultHandler_IsValidMemory((uint32_t)(sp + i)); i++) {
        FaultHandler_Printf("  [SP+%02d] 0x%08lX = 0x%08lX\r\n", 
                            i * 4, (unsigned long)(sp + i), (unsigned long)*(sp + i));
    }
    
#else
    FaultHandler_Printf("Task tracing not enabled\r\n");
#endif
}
