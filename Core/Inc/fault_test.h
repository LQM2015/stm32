#ifndef __FAULT_TEST_H
#define __FAULT_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

// 异常测试函数声明
void test_hardfault_divide_by_zero(void);
void test_hardfault_null_pointer(void);
void test_hardfault_invalid_memory_access(void);
void test_hardfault_undefined_instruction(void);
void test_hardfault_stack_overflow(void);

// Shell命令函数声明
int cmd_fault_test(int argc, char *argv[]);
int cmd_test_null(int argc, char *argv[]);
int cmd_test_memory(int argc, char *argv[]);
int cmd_test_instr(int argc, char *argv[]);
int cmd_test_stack(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __FAULT_TEST_H */
