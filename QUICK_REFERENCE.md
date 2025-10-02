# 快速参考卡

## 编译命令

### Bootloader
```
配置: Debug_Bootloader
宏定义: USE_BOOTLOADER
链接脚本: STM32H750XBHX_BOOTLOADER.ld
输出地址: 0x08000000 (内部Flash)
最大大小: 64KB
```

### 应用程序
```
配置: Debug_ExtFlash
宏定义: (无)
链接脚本: STM32H750XBHX_EXTFLASH.ld
输出地址: 0x90000000 (外部Flash)
最大大小: 32MB
```

## 关键地址

| 名称 | 地址 | 大小 | 用途 |
|------|------|------|------|
| 内部 Flash | 0x08000000 | 128KB | Bootloader |
| Bootloader 区 | 0x08000000 | 64KB | 引导程序 |
| 保留区 | 0x08010000 | 64KB | 未来扩展 |
| QSPI Flash | 0x90000000 | 32MB | 应用程序 |
| DTCMRAM | 0x20000000 | 128KB | 快速数据 |
| RAM_D1 | 0x24000000 | 512KB | 主内存 |
| RAM_D2 | 0x30000000 | 288KB | DMA/外设 |
| RAM_D3 | 0x38000000 | 64KB | 备份域 |

## 必需的代码修改

### 应用程序 main.c

```c
int main(void)
{
  // 重定位向量表（必需！）
  #if !defined(USE_BOOTLOADER)
    SCB->VTOR = 0x90000000;
  #endif
  
  // 其余初始化...
}
```

## 烧录命令

### 使用 STM32CubeProgrammer CLI

**Bootloader**:
```powershell
STM32_Programmer_CLI -c port=SWD -w Debug_Bootloader\H750XBH6.hex -v -s
```

**应用程序**:
```powershell
# 加载外部Flash Loader
STM32_Programmer_CLI -c port=SWD -el "W25Q256.stldr"

# 烧录到外部Flash
STM32_Programmer_CLI -c port=SWD -w Debug_ExtFlash\H750XBH6.hex 0x90000000 -v
```

## 调试设置

### UART 输出
- 端口: UART1
- 波特率: 115200
- 数据位: 8
- 停止位: 1
- 奇偶校验: None

### 预期输出

**Bootloader**:
```
========================================
STM32H750 Bootloader Starting...
========================================
QSPI Flash W25Q256 initialized successfully
QSPI Flash ID: 0xEF4019
...
Jumping to application at address: 0x90000XXX
```

**应用程序**:
```
Starting main function initialization...
System ready, starting FreeRTOS scheduler...
```

## 常见错误

| 错误 | 原因 | 解决方法 |
|------|------|----------|
| 无法跳转 | 应用未烧录 | 烧录应用到0x90000000 |
| Flash ID错误 | 硬件问题 | 检查QSPI连接 |
| 堆栈指针无效 | 链接脚本错误 | 检查EXTFLASH.ld |
| 应用崩溃 | 向量表未重定位 | 添加SCB->VTOR设置 |

## 检查清单

编译前:
- [ ] 选择正确的构建配置
- [ ] 确认宏定义正确
- [ ] 确认链接脚本正确
- [ ] 清理之前的编译输出

烧录前:
- [ ] Bootloader已编译（<64KB）
- [ ] 应用程序已编译
- [ ] 配置了External Loader (应用)
- [ ] 检查了调试器连接

运行后:
- [ ] 串口输出正常
- [ ] Bootloader成功跳转
- [ ] 应用程序正常运行
- [ ] FreeRTOS任务工作正常

## 快速测试

```c
// 在Bootloader中
printf("Bootloader Version: 1.0\r\n");
printf("Flash ID: 0x%06X\r\n", QSPI_W25Qxx_ReadID());

// 在应用程序中
printf("Application Started!\r\n");
printf("VTOR: 0x%08X\r\n", SCB->VTOR);
```

## 文件清单

新增文件:
```
Drivers/Flash_driver/Inc/bootloader.h
Drivers/Flash_driver/Src/bootloader.c
STM32H750XBHX_BOOTLOADER.ld
STM32H750XBHX_EXTFLASH.ld
BOOTLOADER_README.md
CUBEIDE_CONFIG.md
IMPLEMENTATION_SUMMARY.md
QUICK_REFERENCE.md (本文件)
```

修改文件:
```
Core/Src/main.c (添加USE_BOOTLOADER条件编译)
```

---

💡 **提示**: 将此文件打印或保存为桌面快捷方式，方便快速查阅！
