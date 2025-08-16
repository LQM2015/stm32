# STM32H725 调试输出模块使用说明

## 概述
本调试模块提供了完整的串口调试输出功能，支持多级别调试信息、时间戳、彩色输出、十六进制数据打印等功能。

## 功能特性
1. **多级别调试输出**：ERROR、WARN、INFO、DEBUG
2. **彩色输出支持**：基于ANSI转义序列的彩色终端输出
3. **时间戳支持**：可选的毫秒级时间戳
4. **十六进制数据打印**：支持带ASCII显示的彩色hex dump
5. **系统信息打印**：CPU信息、时钟频率等
6. **FreeRTOS任务信息**：任务状态、内存使用等
7. **printf重定向**：支持标准printf函数
8. **美观的格式化输出**：横幅、分隔线等

## 颜色方案
- 🔴 **ERROR**: 红色 - 严重错误信息
- 🟡 **WARN**: 黄色 - 警告信息
- 🟢 **INFO**: 绿色 - 一般信息
- 🔵 **DEBUG**: 青色 - 调试信息
- 💙 **SYSTEM**: 蓝色 - 系统相关信息
- 💚 **SUCCESS**: 亮绿色 - 成功操作
- 💜 **TASK**: 洋红色 - 任务相关信息
- 💙 **MEMORY**: 亮蓝色 - 内存相关信息
- 🔘 **时间戳**: 灰色 - 不干扰主要信息

## 基本使用

### 1. 初始化
```c
// 在main函数中调用
debug_init();
```

### 2. 基本输出
```c
DEBUG_PRINTF("This is a debug message: %d", value);
INFO_PRINTF("System initialized successfully");
WARN_PRINTF("Warning: Low battery voltage");
ERROR_PRINTF("Error: Communication timeout");
SUCCESS_PRINTF("Operation completed successfully");
SYSTEM_PRINTF("System clock configured");
TASK_PRINTF("Task started");
MEMORY_PRINTF("Memory allocation: %d bytes", size);
```

### 3. 十六进制数据打印（彩色）
```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
DEBUG_PRINT_HEX(data, sizeof(data));
DEBUG_PRINT_BUFFER(data, sizeof(data), "Sensor Data");
```

### 4. 系统信息打印
```c
debug_print_system_info();   // CPU信息、时钟等
debug_print_task_info();     // FreeRTOS任务信息
debug_print_memory_info();   // 内存使用情况（带颜色指示）
```

### 5. 调试级别控制
```c
debug_set_level(DEBUG_LEVEL_INFO);  // 只输出INFO及以上级别
```

### 6. 颜色开关控制
```c
debug_set_color_enable(1);  // 启用颜色
debug_set_color_enable(0);  // 禁用颜色
```

### 7. 格式化输出辅助
```c
debug_print_banner("SYSTEM STATUS");  // 打印美观的横幅
debug_print_separator();             // 打印分隔线
```

## 配置选项

### debug.h 中的配置
```c
#define DEBUG_ENABLE           1        // 启用/禁用调试
#define DEBUG_UART             &huart3  // 调试UART
#define DEBUG_BUFFER_SIZE      512      // 调试缓冲区大小
#define DEBUG_TIMESTAMP_ENABLE 1        // 启用时间戳
#define DEBUG_COLOR_ENABLE     1        // 启用颜色输出
```

## 输出格式示例

### 彩色输出模式
```
==================================================
=            DEBUG SYSTEM                        =
==================================================
[00001234] [SYSTEM] Color output: ENABLED
[00001245] [SYSTEM] MCU: STM32H725
[00001256] [SUCCESS] Debug system initialized successfully!
--------------------------------------------------
[00001267] [INFO] MCU ready, starting FreeRTOS scheduler
[00001278] [WARN] This is a warning message test
[00001289] [DEBUG] DefaultTask heartbeat - Counter: 10000
[00001300] [ERROR] Critical Error Occurred!
```

### 内存使用颜色指示
- 🟢 绿色：内存使用 < 60%
- 🟡 黄色：内存使用 60-80%
- 🔴 红色：内存使用 > 80%

## 终端兼容性
支持的终端和工具：
- ✅ **PuTTY** - 需要在设置中启用ANSI颜色
- ✅ **Tera Term** - 支持ANSI颜色
- ✅ **MobaXterm** - 默认支持颜色
- ✅ **VS Code Serial Monitor** - 支持ANSI颜色
- ✅ **Windows Terminal** - 完美支持
- ✅ **Linux/macOS Terminal** - 原生支持

### PuTTY设置
1. Connection -> Data -> Terminal-type string: `xterm-256color`
2. Window -> Colours -> Allow terminal to use xterm 256-colour mode: ✅

## 性能考虑
1. 彩色输出会增加数据传输量（约20-30%）
2. 可通过`DEBUG_COLOR_ENABLE`宏控制
3. 在发布版本中建议禁用DEBUG级别或完全禁用调试
4. 内存使用：每次调试输出占用约512字节缓冲区

## 高级功能
1. **条件编译**：可以完全编译移除调试代码
2. **运行时控制**：可以动态调整调试级别和颜色开关
3. **模块化设计**：易于扩展新的调试类型
4. **线程安全**：在FreeRTOS环境中安全使用

## 注意事项
1. 确保串口终端支持ANSI颜色代码
2. 建议使用115200或更高波特率以获得最佳体验
3. 在中断服务程序中谨慎使用调试输出
4. 长时间运行时注意串口缓冲区管理
