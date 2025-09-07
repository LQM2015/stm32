#include "fault_test.h"
#include "shell_log.h"
#include "shell.h"
#include "stm32h7xx.h"
#include <string.h>

// Test divide by zero error
void test_hardfault_divide_by_zero(void)
{
    SHELL_LOG_SYS_INFO("Testing divide by zero...");
    
    // Enable Usage Fault
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    // Enable divide by zero trap
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    
    SHELL_LOG_SYS_INFO("Enabled divide by zero exception detection");
    
    // Wait for configuration to take effect
    for(volatile int i = 0; i < 100000; i++);
    
    SHELL_LOG_SYS_WARNING("About to perform divide by zero operation, system will trigger UsageFault exception!");
    
    // Method 1: Normal divide by zero
    SHELL_LOG_SYS_INFO("Trying method 1: normal integer divide by zero...");
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;  // Trigger divide by zero exception
    (void)c;  // Avoid compiler warning
    
    SHELL_LOG_SYS_INFO("Method 1 did not trigger exception, trying inline assembly divide by zero...");
    
    // Method 2: Use inline assembly to ensure divide by zero is not optimized away
    __asm volatile(
        "mov r0, #10    \n\t"   // Dividend
        "mov r1, #0     \n\t"   // Divisor is 0
        "sdiv r2, r0, r1 \n\t"  // Execute division, should trigger divide by zero exception
        ::: "r0", "r1", "r2", "memory"
    );
    
    SHELL_LOG_SYS_ERROR("Divide by zero test did not trigger exception! This processor may not support hardware divide by zero detection.");
}

// Test null pointer access
void test_hardfault_null_pointer(void)
{
    SHELL_LOG_SYS_INFO("Testing null pointer access...");
    
    // Enable Memory Management Fault
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Memory Management Fault");
    
    // Wait for configuration to take effect
    for(volatile int i = 0; i < 100000; i++);
    
    SHELL_LOG_SYS_WARNING("About to access invalid memory addresses...");
    
    // Method 1: Try to access address 0x00000000 (might be mapped to Flash)
    SHELL_LOG_SYS_INFO("Method 1: Trying to write to address 0x00000000...");
    volatile uint32_t *p1 = (volatile uint32_t *)0x00000000;
    *p1 = 0x12345678;  // Try to write to address 0 (may be read-only Flash)
    
    // Method 2: Try to access a clearly invalid address in the reserved region
    SHELL_LOG_SYS_INFO("Method 2: Trying to access reserved memory region 0x40000000...");
    volatile uint32_t *p2 = (volatile uint32_t *)0x40000000;  // Reserved region
    volatile uint32_t value = *p2;
    (void)value;
    
    // Method 3: Try to access a region beyond the defined memory map
    SHELL_LOG_SYS_INFO("Method 3: Trying to access invalid high address 0xF0000000...");
    volatile uint32_t *p3 = (volatile uint32_t *)0xF0000000;  // Invalid high address
    *p3 = 0xDEADBEEF;
    
    // Method 4: Try to access an address that should definitely trigger MemManage
    SHELL_LOG_SYS_INFO("Method 4: Trying to access address in reserved system region...");
    volatile uint32_t *p4 = (volatile uint32_t *)0x00001000;  // Try to write to likely read-only area
    *p4 = 0xBADC0DE;
    
    SHELL_LOG_SYS_ERROR("All null pointer/invalid access tests did not trigger exception!");
}

// Test invalid memory access
void test_hardfault_invalid_memory_access(void)
{
    SHELL_LOG_SYS_INFO("Testing invalid memory access...");
    
    // Enable Memory Management Fault
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Memory Management Fault");
    
    SHELL_LOG_SYS_WARNING("About to access invalid memory address, system will trigger MemManage Fault!");
    
    // Access an invalid memory address
    volatile int *invalid_addr = (volatile int *)0xFFFFFFFF;  // Invalid address
    SHELL_LOG_SYS_INFO("Trying to access address 0xFFFFFFFF...");
    volatile int value = *invalid_addr;
    (void)value;
    
    SHELL_LOG_SYS_ERROR("Invalid memory access test did not trigger exception!");
}

// Test privileged instruction
void test_hardfault_privileged_instruction(void)
{
    SHELL_LOG_SYS_INFO("Testing privileged instruction...");
    
    // Enable Usage Fault
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Usage Fault exception");
    
    // Wait for configuration to take effect
    for(volatile int i = 0; i < 100000; i++);
    
    SHELL_LOG_SYS_WARNING("About to execute privileged instruction, system will trigger UsageFault exception!");
    
    // Try to execute privileged instruction in non-privileged mode
    SHELL_LOG_SYS_INFO("Trying to execute MSR instruction to modify PRIMASK...");
    __asm volatile("msr primask, %0" : : "r" (1));  // Fixed MSR instruction syntax
    
    SHELL_LOG_SYS_INFO("Trying to execute CPS instruction...");
    __asm volatile("cpsid i");  // Privileged instruction to disable interrupts
    
    SHELL_LOG_SYS_ERROR("Privileged instruction test did not trigger exception!");
}

// Test forced HardFault (by accessing non-existent peripheral)
void test_hardfault_bus_error(void)
{
    SHELL_LOG_SYS_INFO("Testing bus error...");
    
    // Enable Bus Fault
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Bus Fault exception");
    
    SHELL_LOG_SYS_WARNING("About to access non-existent peripheral address, system will trigger BusFault exception!");
    
    // Access a non-existent peripheral address
    volatile uint32_t *invalid_peripheral = (volatile uint32_t *)0x60000000;  // Non-existent peripheral address
    SHELL_LOG_SYS_INFO("Trying to access address 0x60000000...");
    volatile uint32_t value = *invalid_peripheral;
    (void)value;
    
    SHELL_LOG_SYS_ERROR("Bus error test did not trigger exception!");
}

// Test unaligned access exception
void test_hardfault_unaligned_access(void)
{
    SHELL_LOG_SYS_INFO("Testing unaligned access exception...");
    
    // Enable Usage Fault and unaligned access exception
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;  // Enable unaligned access trap
    
    SHELL_LOG_SYS_INFO("Enabled unaligned access exception detection");
    
    // Wait for configuration to take effect
    for(volatile int i = 0; i < 100000; i++);
    
    SHELL_LOG_SYS_WARNING("About to perform unaligned access, system will trigger UsageFault exception!");
    
    // Create unaligned address (not word-aligned)
    char buffer[8];
    volatile uint32_t *unaligned_ptr = (volatile uint32_t *)(buffer + 1);  // Unaligned by 1 byte
    
    SHELL_LOG_SYS_INFO("Trying to access unaligned address: 0x%08X", (uint32_t)unaligned_ptr);
    *unaligned_ptr = 0x12345678;  // Unaligned access
    
    SHELL_LOG_SYS_ERROR("Unaligned access test did not trigger exception!");
}

// Test undefined instruction
void test_hardfault_undefined_instruction(void)
{
    SHELL_LOG_SYS_INFO("Testing undefined instruction...");
    
    // Enable Usage Fault
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Usage Fault exception");
    
    // Wait for configuration to take effect
    for(volatile int i = 0; i < 100000; i++);
    
    SHELL_LOG_SYS_WARNING("About to execute undefined instruction, system will trigger UsageFault exception!");
    
    // Method 1: Try to execute undefined instruction using inline assembly
    SHELL_LOG_SYS_INFO("Trying method 1: undefined opcode...");
    __asm volatile(".hword 0xDE00");  // Undefined instruction in Thumb mode
    
    // Method 2: Try to execute ARM instruction in Thumb mode
    SHELL_LOG_SYS_INFO("Trying method 2: ARM instruction in Thumb mode...");
    __asm volatile(".hword 0xE1A0, 0x0000");  // ARM NOP instruction in Thumb mode
    
    // Method 3: Try to execute illegal system instruction
    SHELL_LOG_SYS_INFO("Trying method 3: illegal system instruction...");
    __asm volatile("msr control, %0" : : "r" (0xFF));  // Fixed MSR instruction syntax
    
    // Method 4: Jump to odd address (should be even address in Thumb mode)
    SHELL_LOG_SYS_INFO("Trying method 4: jump to odd address...");
    void (*bad_func)(void) = (void(*)(void))0x08000001;  // Odd address
    bad_func();
    
    SHELL_LOG_SYS_ERROR("All methods failed to trigger exception! May need to check processor configuration.");
}

// Test fast stack overflow (direct stack pointer manipulation)
void test_hardfault_stack_direct(void)
{
    SHELL_LOG_SYS_INFO("Testing direct stack access overflow...");
    
    // Enable Memory Management Fault
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    SHELL_LOG_SYS_INFO("Enabled Memory Management Fault");
    
    // Get current stack pointer
    register uint32_t current_sp;
    __asm volatile("mov %0, sp" : "=r" (current_sp));
    SHELL_LOG_SYS_INFO("Current stack pointer: 0x%08X", current_sp);
    
    // Calculate an address that is clearly outside the stack range
    volatile uint32_t *stack_underflow = (volatile uint32_t *)(current_sp - 0x10000);  // Move stack pointer down 64KB
    SHELL_LOG_SYS_WARNING("Trying to access stack underflow address: 0x%08X", (uint32_t)stack_underflow);
    
    // Accessing this address should trigger Memory Management Fault
    *stack_underflow = 0xDEADBEEF;
    
    SHELL_LOG_SYS_ERROR("Stack overflow test did not trigger exception!");
}


// Shell command function - exception test menu
int cmd_fault_test(int argc, char *argv[])
{
    if (argc < 2) {
        SHELL_LOG_SYS_INFO("Exception test commands:");
        SHELL_LOG_SYS_INFO("fault_test null      - test null pointer access");
        SHELL_LOG_SYS_INFO("fault_test memory    - test invalid memory access");
        SHELL_LOG_SYS_INFO("fault_test instr     - test undefined instruction");
        SHELL_LOG_SYS_INFO("fault_test stack     - test stack overflow (recursive)");
        SHELL_LOG_SYS_INFO("fault_test stackfast - test stack overflow (direct)");
        SHELL_LOG_SYS_INFO("fault_test divide    - test divide by zero");
        SHELL_LOG_SYS_INFO("fault_test unalign   - test unaligned access");
        SHELL_LOG_SYS_INFO("fault_test priv      - test privileged instruction");
        SHELL_LOG_SYS_INFO("fault_test bus       - test bus error");
        return 0;
    }
    
    if (strcmp(argv[1], "null") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute null pointer access test, system will enter exception handler!");
        test_hardfault_null_pointer();
    }
    else if (strcmp(argv[1], "memory") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute invalid memory access test, system will enter exception handler!");
        test_hardfault_invalid_memory_access();
    }
    else if (strcmp(argv[1], "instr") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute undefined instruction test, system will enter exception handler!");
        test_hardfault_undefined_instruction();
    }
    else if (strcmp(argv[1], "stackfast") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute fast stack overflow test, system will enter exception handler!");
        test_hardfault_stack_direct();
    }
    else if (strcmp(argv[1], "divide") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute divide by zero test, system may enter exception handler!");
        test_hardfault_divide_by_zero();
    }
    else if (strcmp(argv[1], "unalign") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute unaligned access test, system will enter exception handler!");
        test_hardfault_unaligned_access();
    }
    else if (strcmp(argv[1], "priv") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute privileged instruction test, system will enter exception handler!");
        test_hardfault_privileged_instruction();
    }
    else if (strcmp(argv[1], "bus") == 0) {
        SHELL_LOG_SYS_WARNING("About to execute bus error test, system will enter exception handler!");
        test_hardfault_bus_error();
    }
    else {
        SHELL_LOG_SYS_ERROR("Unknown test type: %s", argv[1]);
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 fault_test, cmd_fault_test, fault exception test\r\nfault_test [null|memory|instr|stack|stackfast|divide|unalign|priv|bus]);

// Individual exception test commands
int cmd_test_null(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("Executing null pointer access test - system will trigger MemManage Fault!");
    test_hardfault_null_pointer();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_null, cmd_test_null, test null pointer access fault);

int cmd_test_memory(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("Executing invalid memory access test - system will trigger HardFault!");
    test_hardfault_invalid_memory_access();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_memory, cmd_test_memory, test invalid memory access fault);

int cmd_test_instr(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("Executing undefined instruction test - system will trigger UsageFault!");
    test_hardfault_undefined_instruction();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_instr, cmd_test_instr, test undefined instruction fault);

