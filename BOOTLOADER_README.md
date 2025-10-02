# STM32H750 Bootloader 使用指南

## 概述

本工程实现了 STM32H750 的 bootloader 功能，允许芯片从内部 Flash 启动后跳转到外部 QSPI Flash (W25Q256) 中运行应用程序。

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│  内部 Flash (128KB)                                      │
│  0x08000000 - 0x0801FFFF                                │
├─────────────────────────────────────────────────────────┤
│  Bootloader (64KB)                                      │
│  0x08000000 - 0x0800FFFF                                │
│  - 初始化 QSPI Flash                                    │
│  - 配置内存映射模式                                      │
│  - 跳转到外部 Flash 应用程序                            │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  外部 QSPI Flash (32MB)                                  │
│  0x90000000 - 0x91FFFFFF                                │
├─────────────────────────────────────────────────────────┤
│  应用程序 (最大 32MB)                                    │
│  0x90000000 - ...                                       │
│  - 主应用程序代码                                        │
│  - FreeRTOS 任务                                        │
│  - 所有应用功能                                          │
└─────────────────────────────────────────────────────────┘
```

## 编译配置

### 方式一：通过 STM32CubeIDE 配置

#### 编译 Bootloader

1. 打开 STM32CubeIDE
2. 右键点击工程 -> Properties -> C/C++ Build -> Settings
3. 在 MCU GCC Compiler -> Preprocessor 中添加宏定义：
   ```
   USE_BOOTLOADER
   ```
4. 在 MCU GCC Linker -> General 中修改 Linker Script：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_BOOTLOADER.ld}
   ```
5. 编译工程，生成 bootloader.hex/bin

#### 编译应用程序

1. 打开 STM32CubeIDE
2. 右键点击工程 -> Properties -> C/C++ Build -> Settings
3. 在 MCU GCC Compiler -> Preprocessor 中**删除或注释掉** `USE_BOOTLOADER` 宏
4. 在 MCU GCC Linker -> General 中修改 Linker Script：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_EXTFLASH.ld}
   ```
5. 编译工程，生成 application.hex/bin

### 方式二：通过配置文件（推荐）

创建两个编译配置：

#### Debug_Bootloader 配置
- 宏定义：`USE_BOOTLOADER`
- 链接脚本：`STM32H750XBHX_BOOTLOADER.ld`
- 输出：bootloader.elf

#### Debug_ExtFlash 配置
- 宏定义：无 `USE_BOOTLOADER`
- 链接脚本：`STM32H750XBHX_EXTFLASH.ld`
- 输出：application.elf

## 烧录流程

### 步骤 1: 烧录 Bootloader 到内部 Flash

```bash
# 使用 STM32CubeProgrammer 或 J-Link
# 将 bootloader.hex 烧录到地址 0x08000000
```

在 STM32CubeIDE 中：
1. 选择 Debug_Bootloader 配置
2. 点击 Run -> Debug 或 Flash
3. Bootloader 将被烧录到内部 Flash

### 步骤 2: 烧录应用程序到外部 QSPI Flash

**方法 A: 使用 STM32CubeProgrammer**

1. 连接 ST-Link/J-Link
2. 打开 STM32CubeProgrammer
3. 选择 External Loader: W25Q256 或对应的 QSPI Flash 型号
4. 将 application.hex 烧录到地址 0x90000000

**方法 B: 使用专用烧录程序（如果有）**

某些开发板可能提供专用的外部 Flash 烧录工具。

**方法 C: 通过应用程序自行烧录**

可以编写一个临时程序，通过 UART/USB 接收应用程序数据并写入 QSPI Flash。

## 工作原理

### Bootloader 启动流程

1. **系统复位**：芯片从内部 Flash 0x08000000 开始执行
2. **初始化**：
   - 配置系统时钟
   - 初始化 UART（用于调试输出）
   - 初始化 QSPI 接口
3. **QSPI Flash 初始化**：
   - 复位 W25Q256 Flash
   - 验证 Flash ID
   - 配置为内存映射模式
4. **准备跳转**：
   - 关闭所有中断
   - 禁用 ICache 和 DCache
   - 关闭 SysTick
5. **跳转到应用程序**：
   - 读取外部 Flash 地址 0x90000000 的堆栈指针
   - 读取地址 0x90000004 的复位向量
   - 设置 MSP（主堆栈指针）
   - 跳转执行

### 应用程序启动流程

1. **从外部 Flash 启动**：应用程序在 0x90000000 开始执行
2. **正常初始化**：应用程序按正常流程初始化所有外设
3. **运行应用**：FreeRTOS 调度器启动，应用程序开始工作

## 调试信息

通过 UART1（115200 baud）可以看到以下调试信息：

### Bootloader 输出示例：
```
========================================
STM32H750 Bootloader Starting...
========================================
QSPI Flash W25Q256 initialized successfully
QSPI Flash ID: 0xEF4019

***************************************
Bootloader: Preparing to jump to external flash application...
QSPI Flash configured in memory mapped mode
Jumping to application at address: 0x90000004
Stack pointer set to: 0x24080000
***************************************
```

## 注意事项

### 1. 堆栈指针验证
应用程序的堆栈指针必须指向有效的 RAM 区域：
- DTCMRAM: 0x20000000 - 0x2001FFFF
- RAM_D1:   0x24000000 - 0x2407FFFF
- RAM_D2:   0x30000000 - 0x30047FFF

### 2. 中断向量表重定位
应用程序需要在启动时重定位中断向量表：
```c
// 在应用程序的 SystemInit() 或 main() 开始处添加
SCB->VTOR = 0x90000000; // 外部 Flash 基地址
```

### 3. QSPI Flash 配置
确保 QSPI 配置正确：
- 时钟频率：建议不超过 133MHz
- 采样模式：根据具体 Flash 芯片调整
- 内存映射模式必须正确配置

### 4. MPU 配置
如果使用 MPU，确保：
- Bootloader 中配置允许访问外部 Flash 区域
- 应用程序中根据需要配置 MPU

### 5. Cache 配置
- Bootloader 在跳转前会禁用 ICache 和 DCache
- 应用程序需要在启动时重新启用 Cache

## 故障排除

### 问题 1: 无法跳转到应用程序

**症状**：Bootloader 初始化成功，但无法跳转

**可能原因**：
1. 应用程序未正确烧录到外部 Flash
2. 应用程序链接脚本配置错误
3. 堆栈指针无效

**解决方法**：
1. 验证外部 Flash 内容（读取 0x90000000 地址）
2. 检查应用程序的链接脚本
3. 确认应用程序使用正确的链接脚本编译

### 问题 2: QSPI Flash 初始化失败

**症状**：显示 "ERROR: QSPI Flash initialization failed!"

**可能原因**：
1. QSPI 硬件连接问题
2. Flash ID 不匹配
3. QSPI 时钟配置错误

**解决方法**：
1. 检查硬件连接
2. 验证 Flash 型号（W25Q256）
3. 调整 QSPI 时钟预分频器

### 问题 3: 应用程序运行异常

**症状**：跳转后应用程序崩溃或无响应

**可能原因**：
1. 中断向量表未正确重定位
2. Cache 配置问题
3. MPU 配置冲突

**解决方法**：
1. 在应用程序中添加 `SCB->VTOR = 0x90000000;`
2. 重新启用 ICache 和 DCache
3. 检查 MPU 配置

## 文件说明

### 源文件
- `Drivers/Flash_driver/Inc/bootloader.h` - Bootloader 头文件
- `Drivers/Flash_driver/Src/bootloader.c` - Bootloader 实现
- `Core/Src/main.c` - 主程序（包含 bootloader 逻辑）

### 链接脚本
- `STM32H750XBHX_BOOTLOADER.ld` - Bootloader 专用链接脚本（64KB 内部 Flash）
- `STM32H750XBHX_EXTFLASH.ld` - 应用程序链接脚本（外部 QSPI Flash）
- `STM32H750XBHX_FLASH.ld` - 原始链接脚本（可选，用于完全内部 Flash 运行）

### 驱动文件
- `Drivers/Flash_driver/Inc/qspi_w25q256.h` - QSPI Flash 驱动头文件
- `Drivers/Flash_driver/Src/qspi_w25q256.c` - QSPI Flash 驱动实现

## 扩展功能建议

1. **固件升级**：添加通过 UART/USB 更新外部 Flash 应用程序的功能
2. **固件验证**：添加 CRC 校验，确保应用程序完整性
3. **多应用支持**：支持在外部 Flash 中存储多个应用程序
4. **安全启动**：添加签名验证，防止未授权的应用程序运行

## 技术支持

如有问题，请参考：
- STM32H750 数据手册
- STM32H7 参考手册
- W25Q256 数据手册
- STM32CubeIDE 用户指南
