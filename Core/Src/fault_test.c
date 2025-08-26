#include "fault_test.h"
#include "shell_log.h"
#include "shell.h"
#include <string.h>

// 测试除零错误 (需要编译器支持并启用除零检测)
void test_hardfault_divide_by_zero(void)
{
    SHELL_LOG_SYS_INFO("测试除零错误...");
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;  // 这可能不会触发异常，取决于编译器和硬件
    (void)c;  // 避免编译器警告
}

// 测试空指针访问
void test_hardfault_null_pointer(void)
{
    SHELL_LOG_SYS_INFO("测试空指针访问...");
    volatile int *p = (volatile int *)0x00000000;  // 空指针
    *p = 0x12345678;  // 写入空指针地址，应该触发 MemManage fault
}

// 测试非法内存访问
void test_hardfault_invalid_memory_access(void)
{
    SHELL_LOG_SYS_INFO("测试非法内存访问...");
    volatile int *p = (volatile int *)0xFFFFFFFF;  // 非法地址
    volatile int value = *p;  // 读取非法地址，应该触发 HardFault
    (void)value;  // 避免编译器警告
}

// 测试未定义指令
void test_hardfault_undefined_instruction(void)
{
    SHELL_LOG_SYS_INFO("测试未定义指令...");
    // 执行一个未定义的指令
    __asm volatile(".word 0xFFFFFFFF");  // 无效的指令编码
}

// 测试栈溢出 (通过无限递归)
static volatile int recursion_depth = 0;
void test_stack_overflow_recursive(void)
{
    recursion_depth++;
    char large_array[1024];  // 占用大量栈空间
    for(int i = 0; i < 1024; i++) {
        large_array[i] = (char)(i & 0xFF);
    }
    
    if(recursion_depth % 100 == 0) {
        SHELL_LOG_SYS_INFO("递归深度: %d", recursion_depth);
    }
    
    test_stack_overflow_recursive();  // 无限递归，最终导致栈溢出
}

// 测试栈溢出的入口函数
void test_hardfault_stack_overflow(void)
{
    SHELL_LOG_SYS_INFO("测试栈溢出...");
    recursion_depth = 0;
    test_stack_overflow_recursive();
}

// Shell命令函数 - 异常测试菜单
int cmd_fault_test(int argc, char *argv[])
{
    if (argc < 2) {
        SHELL_LOG_SYS_INFO("异常测试命令:");
        SHELL_LOG_SYS_INFO("fault_test null      - 测试空指针访问");
        SHELL_LOG_SYS_INFO("fault_test memory    - 测试非法内存访问");
        SHELL_LOG_SYS_INFO("fault_test instr     - 测试未定义指令");
        SHELL_LOG_SYS_INFO("fault_test stack     - 测试栈溢出");
        SHELL_LOG_SYS_INFO("fault_test divide    - 测试除零错误");
        return 0;
    }
    
    if (strcmp(argv[1], "null") == 0) {
        SHELL_LOG_SYS_WARNING("即将执行空指针访问测试，系统将进入异常处理!");
        test_hardfault_null_pointer();
    }
    else if (strcmp(argv[1], "memory") == 0) {
        SHELL_LOG_SYS_WARNING("即将执行非法内存访问测试，系统将进入异常处理!");
        test_hardfault_invalid_memory_access();
    }
    else if (strcmp(argv[1], "instr") == 0) {
        SHELL_LOG_SYS_WARNING("即将执行未定义指令测试，系统将进入异常处理!");
        test_hardfault_undefined_instruction();
    }
    else if (strcmp(argv[1], "stack") == 0) {
        SHELL_LOG_SYS_WARNING("即将执行栈溢出测试，系统将进入异常处理!");
        test_hardfault_stack_overflow();
    }
    else if (strcmp(argv[1], "divide") == 0) {
        SHELL_LOG_SYS_WARNING("即将执行除零错误测试，系统可能进入异常处理!");
        test_hardfault_divide_by_zero();
    }
    else {
        SHELL_LOG_SYS_ERROR("未知的测试类型: %s", argv[1]);
        return -1;
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 fault_test, cmd_fault_test, fault exception test\r\nfault_test [null|memory|instr|stack|divide]);

// 单独的异常测试命令
int cmd_test_null(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("执行空指针访问测试 - 系统将触发MemManage Fault!");
    test_hardfault_null_pointer();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_null, cmd_test_null, test null pointer access fault);

int cmd_test_memory(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("执行非法内存访问测试 - 系统将触发HardFault!");
    test_hardfault_invalid_memory_access();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_memory, cmd_test_memory, test invalid memory access fault);

int cmd_test_instr(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("执行未定义指令测试 - 系统将触发UsageFault!");
    test_hardfault_undefined_instruction();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_instr, cmd_test_instr, test undefined instruction fault);

int cmd_test_stack(int argc, char *argv[])
{
    SHELL_LOG_SYS_WARNING("执行栈溢出测试 - 系统将触发MemManage Fault!");
    test_hardfault_stack_overflow();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 test_stack, cmd_test_stack, test stack overflow fault);
