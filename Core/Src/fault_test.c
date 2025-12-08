/**
  ******************************************************************************
  * @file           : fault_test.c
  * @brief          : Fault Handler Test Commands for Shell
  * @description    : Shell commands to trigger various faults for testing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025
  * All rights reserved.
  *
  * WARNING: These commands will cause the system to crash intentionally!
  *          Use only for testing fault handling.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "shell.h"
#include "shell_log.h"
#include "fault_handler.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdint.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
/* Intentionally undefined function for testing */
typedef void (*invalid_func_t)(void);

/* Private function prototypes -----------------------------------------------*/
static void test_div_by_zero(void);
static void test_invalid_address(void);
static void test_null_pointer(void);
static void test_unaligned_access(void);
static void test_undefined_instruction(void);
static void test_stack_overflow(void);

/* Shell command implementations ---------------------------------------------*/

/**
 * @brief Test command to trigger various faults
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
int cmd_fault_test(int argc, char *argv[])
{
    if (argc < 2) {
        SHELL_LOG_SYS_INFO("Usage: fault <type>");
        SHELL_LOG_SYS_INFO("Types:");
        SHELL_LOG_SYS_INFO("  div0     - Division by zero");
        SHELL_LOG_SYS_INFO("  invalid  - Invalid memory access");
        SHELL_LOG_SYS_INFO("  null     - NULL pointer dereference");
        SHELL_LOG_SYS_INFO("  unalign  - Unaligned access (if trap enabled)");
        SHELL_LOG_SYS_INFO("  undef    - Undefined instruction");
        SHELL_LOG_SYS_INFO("  stack    - Stack overflow");
        SHELL_LOG_SYS_INFO("  info     - Show fault handler status");
        SHELL_LOG_SYS_INFO("  tasks    - Show all FreeRTOS tasks");
        SHELL_LOG_SYS_INFO("  timers   - Show timer task info");
        return 0;
    }
    
    const char *type = argv[1];
    
    if (strcmp(type, "div0") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering division by zero...");
        test_div_by_zero();
    }
    else if (strcmp(type, "invalid") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering invalid memory access...");
        test_invalid_address();
    }
    else if (strcmp(type, "null") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering NULL pointer dereference...");
        test_null_pointer();
    }
    else if (strcmp(type, "unalign") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering unaligned access...");
        test_unaligned_access();
    }
    else if (strcmp(type, "undef") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering undefined instruction...");
        test_undefined_instruction();
    }
    else if (strcmp(type, "stack") == 0) {
        SHELL_LOG_SYS_WARNING("Triggering stack overflow...");
        test_stack_overflow();
    }
    else if (strcmp(type, "info") == 0) {
        SHELL_LOG_SYS_INFO("=== Fault Handler Status ===");
        SHELL_LOG_SYS_INFO("Fault handlers enabled:");
        SHELL_LOG_SYS_INFO("  MemManage: %s", (SCB->SHCSR & SCB_SHCSR_MEMFAULTENA_Msk) ? "YES" : "NO");
        SHELL_LOG_SYS_INFO("  BusFault:  %s", (SCB->SHCSR & SCB_SHCSR_BUSFAULTENA_Msk) ? "YES" : "NO");
        SHELL_LOG_SYS_INFO("  UsageFault:%s", (SCB->SHCSR & SCB_SHCSR_USGFAULTENA_Msk) ? "YES" : "NO");
        SHELL_LOG_SYS_INFO("Trap configuration:");
        SHELL_LOG_SYS_INFO("  DIV_0_TRP: %s", (SCB->CCR & SCB_CCR_DIV_0_TRP_Msk) ? "YES" : "NO");
        SHELL_LOG_SYS_INFO("  UNALIGN_TRP: %s", (SCB->CCR & SCB_CCR_UNALIGN_TRP_Msk) ? "YES" : "NO");
    }
    else if (strcmp(type, "tasks") == 0) {
        SHELL_LOG_SYS_INFO("=== FreeRTOS Task List ===");
        FaultHandler_DumpTasks();
    }
    else if (strcmp(type, "timers") == 0) {
        SHELL_LOG_SYS_INFO("=== FreeRTOS Timer Info ===");
        FaultHandler_DumpTimers();
    }
    else {
        SHELL_LOG_SYS_ERROR("Unknown fault type: %s", type);
        return -1;
    }
    
    return 0;
}

/* Register shell command */
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 fault, cmd_fault_test, Test fault handler by triggering various faults);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Trigger division by zero
 */
static void test_div_by_zero(void)
{
    volatile int a = 100;
    volatile int b = 0;
    volatile int c;
    
    /* Prevent compiler optimization */
    c = a / b;  /* This will trigger UsageFault (DIVBYZERO) */
    
    (void)c;  /* Suppress unused warning */
}

/**
 * @brief Trigger invalid memory access
 */
static void test_invalid_address(void)
{
    /* Access an invalid memory address */
    volatile uint32_t *invalid_ptr = (volatile uint32_t *)0xFFFFFFFF;
    volatile uint32_t value;
    
    value = *invalid_ptr;  /* This will trigger BusFault or HardFault */
    
    (void)value;
}

/**
 * @brief Trigger NULL pointer dereference
 */
static void test_null_pointer(void)
{
    volatile uint32_t *null_ptr = NULL;
    volatile uint32_t value;
    
    value = *null_ptr;  /* This will trigger a fault */
    
    (void)value;
}

/**
 * @brief Trigger unaligned access
 */
static void test_unaligned_access(void)
{
    /* Create an unaligned address */
    uint8_t buffer[8] = {0};
    volatile uint32_t *unaligned_ptr = (volatile uint32_t *)(buffer + 1);
    volatile uint32_t value;
    
    /* Enable unaligned access trap first */
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();
    
    value = *unaligned_ptr;  /* This will trigger UsageFault (UNALIGNED) */
    
    (void)value;
}

/**
 * @brief Trigger undefined instruction
 */
static void test_undefined_instruction(void)
{
    /* 0xFFFFFFFF is an undefined instruction on ARM Cortex-M */
    /* We'll execute it by jumping to a buffer containing this value */
    
    /* Create a buffer with undefined instruction in RAM */
    /* Note: Must be in RAM and aligned, plus 1 for Thumb mode */
    static uint16_t undef_instr[] __attribute__((aligned(4))) = {
        0xDEFF,  /* Undefined instruction (permanently undefined) */
        0xDEFF
    };
    
    /* Cast to function pointer with Thumb bit set */
    invalid_func_t func = (invalid_func_t)((uint32_t)undef_instr | 1);
    
    /* Execute undefined instruction */
    func();  /* This will trigger UsageFault (UNDEFINSTR) */
}

/**
 * @brief Trigger stack overflow (recursive function)
 * @note Intentionally infinite recursion to test stack overflow handling
 */
static volatile int stack_depth = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
static void recursive_overflow(void)
{
    volatile uint8_t local_buffer[256];  /* Use stack space */
    
    stack_depth++;
    
    /* Fill buffer to prevent optimization */
    for (int i = 0; i < 256; i++) {
        local_buffer[i] = (uint8_t)stack_depth;
    }
    
    /* Recurse until stack overflow - this is intentional for testing */
    recursive_overflow();
    
    /* This line is never reached, but prevents tail-call optimization */
    (void)local_buffer[0];
}
#pragma GCC diagnostic pop

static void test_stack_overflow(void)
{
    stack_depth = 0;
    recursive_overflow();  /* This will eventually overflow the stack */
}
