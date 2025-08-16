# Letter Shell 移植说明

## 概述

本文档说明了如何在STM32H725工程中使用Letter Shell 3.2.4版本。Letter Shell是一个功能强大的嵌入式shell，支持命令行交互、变量操作、函数调用等功能。

## 文件结构

```
Shell/
├── inc/
│   ├── shell.h              # Shell核心头文件
│   ├── shell_cfg.h          # Shell默认配置
│   ├── shell_cfg_user.h     # 用户自定义配置
│   ├── shell_ext.h          # Shell扩展功能
│   ├── shell_port.h         # 移植层头文件
│   └── shell_commands.h     # 自定义命令头文件
└── src/
    ├── shell.c              # Shell核心实现
    ├── shell_cmd_list.c     # 命令列表
    ├── shell_ext.c          # Shell扩展功能实现
    ├── shell_port.c         # 移植层实现
    └── shell_commands.c     # 自定义命令实现
```

## 配置说明

### 1. 编译器配置

在你的IDE中添加以下包含路径：
- `Shell/inc`
- `Shell/src`

添加编译宏定义：
- `SHELL_CFG_USER="shell_cfg_user.h"`

### 2. 链接器配置

对于使用Keil MDK的情况，需要在链接器选项中添加：
```
--keep shellCommand*
```

对于使用GCC的情况，需要在链接脚本(.ld文件)中添加：
```ld
.rodata :
{
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    
    /* Shell command section */
    _shell_command_start = .;
    KEEP (*(shellCommand))
    _shell_command_end = .;
    
    . = ALIGN(4);
} >FLASH
```

### 3. 硬件配置

Shell使用UART3作为通信接口：
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：无
- 流控：无

## 使用方法

### 1. 初始化

在main.c中调用shell初始化函数：

```c
#include "shell_port.h"

int main(void)
{
    // ... 其他初始化代码
    
    // 初始化Shell
    shell_init();
    
    // ... 启动FreeRTOS调度器
}
```

### 2. 自定义命令

可以通过以下方式添加自定义命令：

```c
#include "shell.h"

// 命令实现函数
int my_command(int argc, char *argv[])
{
    printf("Hello from my command!\r\n");
    return 0;
}

// 导出命令
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), 
                 mycmd, my_command, my custom command);
```

### 3. 变量导出

可以导出变量供shell访问：

```c
static int my_variable = 100;
SHELL_EXPORT_VAR(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_VAR_INT), 
                 myvar, &my_variable, my variable);
```

## 内置命令

移植后的shell包含以下内置命令：

- `help` - 显示帮助信息
- `sysinfo` - 显示系统信息
- `meminfo` - 显示内存使用情况
- `taskinfo` - 显示任务信息
- `reboot` - 重启系统
- `clear` - 清屏
- `led` - LED控制（需要硬件支持）
- `clocktest` - 时钟配置测试
- `version` - 显示版本信息
- `hexdump` - 十六进制内存dump

## 使用示例

连接串口终端（如SecureCRT、Tera Term等），配置为115200波特率，然后可以输入以下命令：

```
stm32h725:/$ help
stm32h725:/$ sysinfo
stm32h725:/$ meminfo
stm32h725:/$ taskinfo
stm32h725:/$ version
stm32h725:/$ hexdump 0x08000000 256
```

## 特性

- 支持命令自动补全（Tab键）
- 支持历史命令（上下箭头键）
- 支持变量操作
- 支持函数签名和参数类型检查
- 支持权限管理
- 支持多用户
- 线程安全设计
- 支持彩色输出

## 注意事项

1. Shell任务优先级设置为3，栈大小为2048字节
2. 使用递归互斥锁保证线程安全
3. UART接收使用中断+队列方式，提高响应性能
4. 支持ANSI转义序列，建议使用支持彩色的终端软件

## 故障排除

1. 如果命令无法识别，检查链接器配置是否正确
2. 如果shell无响应，检查UART配置和中断是否正常
3. 如果内存不足，可以调整shell缓冲区大小和任务栈大小
4. 如果编译错误，检查包含路径和宏定义是否正确

## 扩展功能

Letter Shell还支持以下扩展功能（需要额外配置）：
- 文件系统支持
- 日志系统
- Telnet支持
- 游戏模块
- C++支持

详细信息请参考Letter Shell官方文档。