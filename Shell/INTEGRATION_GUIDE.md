# Letter Shell 集成指南

## STM32CubeIDE 集成步骤

### 1. 添加源文件到项目

1. 在STM32CubeIDE中右键点击项目
2. 选择 `Properties` -> `C/C++ Build` -> `Settings`
3. 在 `Tool Settings` 标签下，找到 `MCU GCC Compiler` -> `Include paths`
4. 添加以下包含路径：
   - `../Shell/inc`
   - `../Shell/src`

### 2. 添加编译宏定义

在 `MCU GCC Compiler` -> `Preprocessor` -> `Defined symbols` 中添加：
```
SHELL_CFG_USER="shell_cfg_user.h"
```

### 3. 修改链接脚本

打开你的链接脚本文件（通常是 `STM32H725AEIX_FLASH.ld`），在 `.rodata` 段中添加：

```ld
.rodata :
{
  . = ALIGN(4);
  *(.rodata)
  *(.rodata*)
  
  /* Shell command section */
  . = ALIGN(4);
  _shell_command_start = .;
  KEEP (*(shellCommand))
  _shell_command_end = .;
  
  . = ALIGN(4);
} >FLASH
```

### 4. 添加源文件到构建

确保以下文件被包含在构建中：
- `Shell/src/shell.c`
- `Shell/src/shell_port.c`
- `Shell/src/shell_commands.c`
- `Shell/src/shell_cmd_list.c`
- `Shell/src/shell_ext.c`

## Keil MDK 集成步骤

### 1. 添加文件组

1. 在项目管理器中右键点击项目
2. 选择 `Add Group...` 创建 `Shell` 组
3. 添加以下源文件到 `Shell` 组：
   - `Shell/src/shell.c`
   - `Shell/src/shell_port.c`
   - `Shell/src/shell_commands.c`
   - `Shell/src/shell_cmd_list.c`
   - `Shell/src/shell_ext.c`

### 2. 配置包含路径

1. 右键项目选择 `Options for Target...`
2. 在 `C/C++` 标签页的 `Include Paths` 中添加：
   - `Shell\inc`
   - `Shell\src`

### 3. 添加预处理器定义

在 `C/C++` 标签页的 `Preprocessor Symbols` -> `Define` 中添加：
```
SHELL_CFG_USER="shell_cfg_user.h"
```

### 4. 配置链接器

在 `Linker` 标签页的 `Misc controls` 中添加：
```
--keep shellCommand*
```

## IAR EWARM 集成步骤

### 1. 添加文件到项目

1. 右键项目选择 `Add` -> `Add Group...` 创建 `Shell` 组
2. 添加所有Shell源文件到该组

### 2. 配置包含路径

1. 右键项目选择 `Options...`
2. 在 `C/C++ Compiler` -> `Preprocessor` -> `Additional include directories` 中添加：
   - `$PROJ_DIR$\Shell\inc`
   - `$PROJ_DIR$\Shell\src`

### 3. 添加预处理器定义

在 `C/C++ Compiler` -> `Preprocessor` -> `Defined symbols` 中添加：
```
SHELL_CFG_USER="shell_cfg_user.h"
```

### 4. 配置链接器

在链接脚本中添加shell命令段的定义。

## 验证集成

### 1. 编译测试

编译项目，确保没有编译错误。常见问题：
- 包含路径不正确
- 预处理器宏定义缺失
- 链接脚本配置错误

### 2. 运行测试

1. 烧录程序到STM32H725
2. 连接串口调试工具（115200波特率）
3. 应该看到shell启动信息
4. 输入 `help` 命令测试

### 3. 功能测试

测试以下命令：
```bash
help          # 显示帮助
sysinfo       # 系统信息
meminfo       # 内存信息
taskinfo      # 任务信息
version       # 版本信息
clear         # 清屏
```

## 常见问题解决

### 1. 编译错误：找不到头文件

**解决方案：**
- 检查包含路径配置
- 确保Shell/inc目录已添加到包含路径

### 2. 链接错误：未定义的符号

**解决方案：**
- 检查所有源文件是否已添加到项目
- 确保链接脚本正确配置了shell命令段

### 3. Shell无响应

**解决方案：**
- 检查UART配置（波特率、引脚配置）
- 确保shell_init()已在main函数中调用
- 检查FreeRTOS任务是否正常创建

### 4. 命令无法识别

**解决方案：**
- 检查链接脚本中的shell命令段配置
- 确保使用了正确的链接器选项（Keil需要--keep参数）

### 5. 内存不足

**解决方案：**
- 调整shell缓冲区大小（SHELL_BUFFER_SIZE）
- 调整shell任务栈大小（SHELL_TASK_STACK_SIZE）
- 检查FreeRTOS堆大小配置

## 性能优化建议

### 1. 内存优化

- 根据实际需求调整缓冲区大小
- 合理设置历史命令数量
- 优化任务栈大小

### 2. 响应性优化

- 使用中断接收UART数据
- 合理设置任务优先级
- 使用队列缓存接收数据

### 3. 功能裁剪

根据需求在shell_cfg_user.h中关闭不需要的功能：
- 关闭伴生对象功能
- 关闭尾行模式
- 关闭函数签名功能
- 关闭数组参数支持

## 扩展开发

### 1. 添加自定义命令

参考shell_commands.c中的示例，添加新的命令函数并使用SHELL_EXPORT_CMD宏导出。

### 2. 添加变量导出

使用SHELL_EXPORT_VAR宏导出需要在shell中访问的变量。

### 3. 集成其他模块

可以将其他功能模块的控制接口导出为shell命令，方便调试和测试。

## 技术支持

如果遇到问题，可以：
1. 查看Letter Shell官方文档
2. 检查本文档的常见问题部分
3. 参考示例代码和配置文件