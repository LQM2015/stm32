# STM32 异常处理系统使用说明

## 概述
本系统为STM32H7系列微控制器提供了全面的异常处理和调试信息转储功能。当MCU发生异常时，系统会自动捕获并打印详细的寄存器状态、故障分析和FreeRTOS任务信息。

## 功能特性
- 捕获所有主要的ARM Cortex-M异常类型：
  - HardFault（硬件故障）
  - MemManage Fault（内存管理故障）
  - BusFault（总线故障）
  - UsageFault（用法故障）
  - DebugMon（调试监控异常）

- 详细的故障信息输出：
  - CPU寄存器状态（R0-R12, LR, PC, PSR）
  - 系统故障状态寄存器（CFSR, HFSR, DFSR, AFSR）
  - 故障地址寄存器（MMAR, BFAR）
  - 中文故障类型分析
  - FreeRTOS任务状态信息

## 文件说明
- `Core/Inc/fault_handler.h` - 异常处理头文件
- `Core/Src/fault_handler.c` - 异常处理实现文件
- `Core/Inc/fault_test.h` - 异常测试头文件
- `Core/Src/fault_test.c` - 异常测试实现文件
- `Core/Src/stm32h7xx_it.c` - 中断服务程序（已修改）
- `Core/Inc/FreeRTOSConfig.h` - FreeRTOS配置（已更新）

## 使用方法

### 1. 编译项目
使用您现有的编译命令：
```bash
./compile.bat rebuild
```

### 2. 烧录固件
```bash
flash.bat flash
```

### 3. 查看调试日志
```bash
./view_log.ps1
```

### 4. 测试异常处理
在您的代码中包含测试头文件：
```c
#include "fault_test.h"
```

#### 方法1：通过串口命令测试（推荐）
连接串口后，您可以使用以下命令来测试异常处理：

```bash
# 查看所有可用的异常测试命令
fault_test

# 测试空指针访问（MemManage Fault）
fault_test null
# 或者使用单独命令
test_null

# 测试非法内存访问（HardFault）
fault_test memory
# 或者使用单独命令
test_memory

# 测试未定义指令（UsageFault）
fault_test instr
# 或者使用单独命令
test_instr

# 测试栈溢出（MemManage Fault）
fault_test stack
# 或者使用单独命令
test_stack

# 测试除零错误（可能不触发异常）
fault_test divide
```

#### 方法2：在代码中直接调用测试函数
```c
// 测试空指针访问
test_hardfault_null_pointer();

// 测试非法内存访问
test_hardfault_invalid_memory_access();

// 测试未定义指令
test_hardfault_undefined_instruction();

// 测试栈溢出
test_hardfault_stack_overflow();
```

## 可用的串口测试命令

| 命令 | 描述 | 触发的异常类型 |
|------|------|----------------|
| `fault_test` | 显示所有可用的异常测试选项 | - |
| `fault_test null` 或 `test_null` | 测试空指针访问 | MemManage Fault |
| `fault_test memory` 或 `test_memory` | 测试非法内存访问 | HardFault |
| `fault_test instr` 或 `test_instr` | 测试未定义指令 | UsageFault |
| `fault_test stack` 或 `test_stack` | 测试栈溢出 | MemManage Fault |
| `fault_test divide` | 测试除零错误 | 可能不触发异常 |

## 使用示例

### 通过串口命令测试
1. 连接串口，打开串口终端
2. 输入命令测试异常处理：

```
# 查看帮助
letter@STM32H725> fault_test
[INFO][SYS] 异常测试命令:
[INFO][SYS] fault_test null      - 测试空指针访问
[INFO][SYS] fault_test memory    - 测试非法内存访问
[INFO][SYS] fault_test instr     - 测试未定义指令
[INFO][SYS] fault_test stack     - 测试栈溢出
[INFO][SYS] fault_test divide    - 测试除零错误

# 执行空指针访问测试
letter@STM32H725> test_null
[WARNING][SYS] 执行空指针访问测试 - 系统将触发MemManage Fault!
[INFO][SYS] 测试空指针访问...

========================================
        MCU 异常故障信息转储           
========================================
[ERROR][SYS] CPU 寄存器状态:
[ERROR][SYS] R0  = 0x00000000
[ERROR][SYS] R1  = 0x12345678
...
```

## 输出示例
当发生异常时，您将看到类似以下的输出：

```
========================================
        MCU 异常故障信息转储           
========================================
CPU 寄存器状态:
R0  = 0x20000000
R1  = 0x12345678
R2  = 0x00000000
R3  = 0xFFFFFFFF
R12 = 0x08001234
LR  = 0x08001000 (异常前的链接寄存器)
PC  = 0x08001004 (故障发生地址)
PSR = 0x21000000 (程序状态寄存器)

系统故障状态寄存器:
CFSR = 0x00000082 (可配置故障状态寄存器)
HFSR = 0x40000000 (硬故障状态寄存器)
DFSR = 0x00000000 (调试故障状态寄存器)
AFSR = 0x00000000 (辅助故障状态寄存器)

故障类型分析:
Memory Management Fault detected:
  - 数据访问违规
  - MMAR 寄存器包含有效的故障地址

FreeRTOS 任务状态信息:
========================================
任务名称         状态 优先级 剩余栈 任务号
----------------------------------------
IDLE            就绪      0    112      1
main_task       运行      1    256      2
shell_task      阻塞      2    128      3

当前运行任务: main_task
========================================
系统已停止，请检查故障原因并重启
========================================
```

## 故障分析指南

### 常见故障类型
1. **Memory Management Fault**
   - 指令访问违规：尝试执行不可执行内存区域的代码
   - 数据访问违规：访问受保护的内存区域
   - 栈操作错误：栈溢出或栈损坏

2. **Bus Fault**
   - 指令总线错误：从无效地址获取指令
   - 数据总线错误：从无效地址读写数据
   - 总线超时：外设访问超时

3. **Usage Fault**
   - 未定义指令：执行了CPU不支持的指令
   - 非法状态：ARM/Thumb状态错误
   - 除零错误：除法指令除零（需要启用）
   - 未对齐访问：访问未对齐的内存地址

### 调试建议
1. 检查PC寄存器值，定位故障发生的代码位置
2. 分析故障类型，确定问题的根本原因
3. 检查当前运行的任务，可能是特定任务的问题
4. 查看任务的剩余栈空间，判断是否有栈溢出
5. 使用调试器设置断点，单步调试问题代码

## 注意事项
- 异常处理函数会使用shell_printf输出信息，确保串口已正确初始化
- 在异常处理过程中，系统会停止运行，需要重启才能继续
- 测试函数仅用于验证异常处理功能，请勿在生产代码中使用
- 确保有足够的栈空间用于异常处理函数的执行

## 配置选项
在 `FreeRTOSConfig.h` 中已启用以下配置：
- `INCLUDE_uxTaskGetSystemState` - 用于获取任务状态信息
- `INCLUDE_xTaskGetCurrentTaskHandle` - 用于获取当前任务句柄
- `INCLUDE_pcTaskGetTaskName` - 用于获取任务名称

如需修改异常处理行为，可以编辑 `fault_handler.c` 文件。
