# Bootloader 实现总结

## 已完成的工作

### ✅ 1. 创建了 Bootloader 核心文件

**文件**:
- `Drivers/Flash_driver/Inc/bootloader.h` - Bootloader 头文件
- `Drivers/Flash_driver/Src/bootloader.c` - Bootloader 实现

**功能**:
- 初始化 QSPI Flash (W25Q256)
- 验证 Flash ID
- 配置内存映射模式
- 跳转到外部 Flash 应用程序 (0x90000000)
- 完整的错误处理和调试输出

### ✅ 2. 修改了主程序

**文件**: `Core/Src/main.c`

**改动**:
- 添加条件编译支持 (`#ifdef USE_BOOTLOADER`)
- Bootloader 模式：初始化并跳转到外部 Flash
- 正常模式：运行 FreeRTOS 应用程序
- 保持了原有的应用程序功能

### ✅ 3. 创建了链接脚本

**文件**:
1. `STM32H750XBHX_BOOTLOADER.ld`
   - 内部 Flash: 64KB (0x08000000 - 0x0800FFFF)
   - 用于 Bootloader 程序
   
2. `STM32H750XBHX_EXTFLASH.ld`
   - 外部 QSPI Flash: 32MB (0x90000000 - 0x91FFFFFF)
   - 用于主应用程序
   
3. `STM32H750XBHX_FLASH.ld` (原有)
   - 内部 Flash: 128KB (0x08000000 - 0x0801FFFF)
   - 备用选项

### ✅ 4. 创建了配置文档

**文件**:
1. `BOOTLOADER_README.md`
   - 详细的使用指南
   - 系统架构说明
   - 故障排除指南
   - 扩展功能建议

2. `CUBEIDE_CONFIG.md`
   - STM32CubeIDE 配置步骤
   - 构建配置说明
   - 调试器设置
   - 烧录流程
   - 完整的检查清单

### ✅ 5. QSPI Flash 驱动

**文件**: 
- `Drivers/Flash_driver/Inc/qspi_w25q256.h`
- `Drivers/Flash_driver/Src/qspi_w25q256.c`

**功能** (已存在):
- QSPI 初始化
- 内存映射模式
- Flash 读写擦除
- 性能优化的 DMA 支持

## 使用流程

### 编译 Bootloader

```bash
# 在 STM32CubeIDE 中
1. Build Configurations → Set Active → Debug_Bootloader
2. 添加宏定义: USE_BOOTLOADER
3. 链接脚本: STM32H750XBHX_BOOTLOADER.ld
4. Project → Build
```

### 编译应用程序

```bash
# 在 STM32CubeIDE 中
1. Build Configurations → Set Active → Debug_ExtFlash
2. 确保没有 USE_BOOTLOADER 宏
3. 链接脚本: STM32H750XBHX_EXTFLASH.ld
4. Project → Build
```

### 烧录

```bash
# 1. 烧录 Bootloader 到内部 Flash (0x08000000)
Run → Debug (选择 Bootloader 配置)

# 2. 烧录应用程序到外部 Flash (0x90000000)
# 需要配置 External Loader: W25Q256
Run → Debug (选择 ExtFlash 配置)
```

## 工作原理

```
1. 芯片复位
   ↓
2. 从 0x08000000 启动 (内部 Flash)
   ↓
3. Bootloader 初始化
   - 配置系统时钟
   - 初始化 UART
   - 初始化 QSPI
   ↓
4. QSPI Flash 配置
   - 验证 Flash ID (0xEF4019)
   - 配置为内存映射模式
   - 映射到 0x90000000
   ↓
5. 准备跳转
   - 禁用中断
   - 关闭 Cache
   - 关闭 SysTick
   ↓
6. 跳转到应用程序
   - 设置堆栈指针 (MSP)
   - 跳转到 0x90000004 (Reset Handler)
   ↓
7. 应用程序运行
   - 重定位向量表到 0x90000000
   - 正常初始化
   - 启动 FreeRTOS
```

## 内存布局

```
┌─────────────────────────────────────────┐
│ 内部 Flash (128KB)                       │
│ 0x08000000 ─────────────────────────┐   │
│                                     │   │
│  Bootloader (64KB)                  │   │
│  ├─ 初始化代码                       │   │
│  ├─ QSPI 驱动                       │   │
│  └─ 跳转逻辑                        │   │
│                                     │   │
│ 0x0800FFFF ─────────────────────────┘   │
│                                         │
│  保留 (64KB)                            │
│  可用于未来扩展                          │
│                                         │
│ 0x0801FFFF ─────────────────────────    │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│ 外部 QSPI Flash (32MB)                   │
│ 0x90000000 ─────────────────────────┐   │
│                                     │   │
│  应用程序                            │   │
│  ├─ 向量表                          │   │
│  ├─ 代码段 (.text)                  │   │
│  ├─ 只读数据 (.rodata)              │   │
│  └─ 初始化数据 (.data, LMA)         │   │
│                                     │   │
│ ... (最多 32MB)                     │   │
│                                     │   │
│ 0x91FFFFFF ─────────────────────────┘   │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│ RAM (1056KB total)                      │
├─────────────────────────────────────────┤
│ DTCMRAM: 0x20000000 - 0x2001FFFF (128K)│
│ RAM_D1:  0x24000000 - 0x2407FFFF (512K)│
│ RAM_D2:  0x30000000 - 0x30047FFF (288K)│
│ RAM_D3:  0x38000000 - 0x3800FFFF (64K) │
│                                         │
│ 用于:                                   │
│ - 堆栈 (.stack)                         │
│ - 堆 (.heap)                            │
│ - 已初始化数据 (.data, VMA)             │
│ - 未初始化数据 (.bss)                   │
│ - FreeRTOS 任务栈                       │
└─────────────────────────────────────────┘
```

## 关键配置点

### 1. 预处理器宏

```c
// Bootloader 模式
#define USE_BOOTLOADER

// 应用程序模式
// 不定义 USE_BOOTLOADER
```

### 2. 向量表重定位（应用程序）

在 `main.c` 或 `system_stm32h7xx.c` 中：

```c
#if !defined(USE_BOOTLOADER)
    SCB->VTOR = 0x90000000;  // 重定位到外部 Flash
#endif
```

### 3. 链接脚本选择

| 模式 | 链接脚本 | 目标地址 |
|------|---------|----------|
| Bootloader | STM32H750XBHX_BOOTLOADER.ld | 0x08000000 |
| Application | STM32H750XBHX_EXTFLASH.ld | 0x90000000 |
| Internal (备用) | STM32H750XBHX_FLASH.ld | 0x08000000 |

### 4. QSPI 配置

```c
// 内存映射地址
#define W25Qxx_Mem_Addr  0x90000000

// Flash ID (W25Q256)
#define W25Qxx_FLASH_ID  0xEF4019
```

## 调试输出示例

### Bootloader 启动:

```
========================================
STM32H750 Bootloader Starting...
========================================
QSPI Flash W25Q256 initialized successfully
QSPI Flash ID: 0xEF4019

***************************************
Bootloader: Preparing to jump to external flash application...
QSPI Flash configured in memory mapped mode
Jumping to application at address: 0x90000XXX
Stack pointer set to: 0x24080000
***************************************
```

### 应用程序启动:

```
Starting main function initialization...
UART1 initialized at 115200 baud
QUADSPI initialized
DMA and MDMA initialized
GPIO initialized
System ready, starting FreeRTOS scheduler...
```

## 注意事项

### ⚠️ 重要提醒

1. **向量表重定位**: 应用程序必须重定位向量表到 0x90000000
2. **External Loader**: 烧录外部 Flash 需要配置 W25Q256 loader
3. **堆栈指针**: 应用程序的堆栈指针必须指向有效 RAM 区域
4. **Cache**: Bootloader 跳转前会禁用 Cache，应用程序需重新启用
5. **调试器**: 调试外部 Flash 应用时，确保调试器支持外部存储器访问

### 🔍 验证方法

1. **编译大小**: Bootloader 应小于 64KB
2. **串口输出**: 查看完整的启动日志
3. **内存读取**: 使用 STM32CubeProgrammer 读取 0x90000000
4. **应用运行**: 确认 FreeRTOS 任务正常运行

## 下一步建议

- [ ] 添加固件升级功能（通过 UART/USB）
- [ ] 添加 CRC 校验（验证应用程序完整性）
- [ ] 添加固件版本管理
- [ ] 实现 A/B 分区（故障恢复）
- [ ] 添加加密和签名（安全启动）
- [ ] 实现 OTA 更新功能

## 技术支持

如需帮助，请参考：
- `BOOTLOADER_README.md` - 详细使用指南
- `CUBEIDE_CONFIG.md` - IDE 配置说明
- STM32H750 参考手册
- W25Q256 数据手册

---

**版本**: 1.0  
**日期**: 2025-10-02  
**状态**: ✅ 实现完成，待测试验证
