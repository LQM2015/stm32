# STM32CubeIDE 工程配置说明

## 快速配置指南

### 1. 创建两个构建配置

在 STM32CubeIDE 中：

1. 右键点击项目 → `Build Configurations` → `Manage...`
2. 点击 `New...` 创建新配置：
   - 名称：`Debug_Bootloader`
   - 复制自：`Debug`
3. 再次点击 `New...` 创建：
   - 名称：`Debug_ExtFlash`
   - 复制自：`Debug`

### 2. 配置 Bootloader 构建

选择 `Debug_Bootloader` 配置：

#### 2.1 添加预处理器宏定义

1. 右键项目 → `Properties`
2. `C/C++ Build` → `Settings`
3. `MCU GCC Compiler` → `Preprocessor`
4. 在 `Define symbols (-D)` 中添加：
   ```
   USE_BOOTLOADER
   ```

#### 2.2 修改链接脚本

1. 在相同的 `Settings` 窗口
2. `MCU GCC Linker` → `General`
3. `Linker Script (-T)` 修改为：
   ```
   ../STM32H750XBHX_BOOTLOADER.ld
   ```
   或者使用变量：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_BOOTLOADER.ld}
   ```

#### 2.3 优化设置（可选）

为了使 Bootloader 尽可能小：

1. `MCU GCC Compiler` → `Optimization`
2. 选择 `Optimize for size (-Os)`

### 3. 配置外部 Flash 应用程序构建

选择 `Debug_ExtFlash` 配置：

#### 3.1 确保没有 Bootloader 宏

1. 右键项目 → `Properties`
2. `C/C++ Build` → `Settings`
3. `MCU GCC Compiler` → `Preprocessor`
4. 确保 `Define symbols (-D)` 中**没有** `USE_BOOTLOADER`

#### 3.2 修改链接脚本

1. `MCU GCC Linker` → `General`
2. `Linker Script (-T)` 修改为：
   ```
   ../STM32H750XBHX_EXTFLASH.ld
   ```
   或者使用变量：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_EXTFLASH.ld}
   ```

#### 3.3 添加向量表重定位（重要！）

在应用程序的 `system_stm32h7xx.c` 文件的 `SystemInit()` 函数中添加：

```c
void SystemInit(void)
{
  /* FPU settings, DWT, etc. */
  
#if !defined(USE_BOOTLOADER)
  /* 重定位中断向量表到外部 Flash */
  SCB->VTOR = 0x90000000;
#endif
  
  /* 其余初始化代码 */
}
```

或者在 `main.c` 的 `main()` 函数开始处添加：

```c
int main(void)
{
#if !defined(USE_BOOTLOADER)
  /* 重定位中断向量表到外部 Flash */
  SCB->VTOR = 0x90000000;
#endif

  /* USER CODE BEGIN 1 */
  // ...
}
```

### 4. 添加必要的源文件

确保以下文件包含在编译中：

#### 4.1 Bootloader 源文件
- `Drivers/Flash_driver/Src/bootloader.c`
- `Drivers/Flash_driver/Src/qspi_w25q256.c`

#### 4.2 包含路径
在 `MCU GCC Compiler` → `Include paths` 中添加：
```
../Drivers/Flash_driver/Inc
```

### 5. 调试器配置

#### 5.1 Bootloader 调试配置

1. `Run` → `Debug Configurations...`
2. 双击 `STM32 C/C++ Application` 创建新配置
3. 命名为 `H750XBH6 Bootloader Debug`
4. `Main` 标签：
   - C/C++ Application: `Debug_Bootloader/H750XBH6.elf`
5. `Debugger` 标签：
   - Debug probe: ST-LINK (OpenOCD) 或 J-Link
   - Reset Mode: Software system reset

#### 5.2 外部 Flash 应用程序调试配置

1. `Run` → `Debug Configurations...`
2. 创建新的 `STM32 C/C++ Application` 配置
3. 命名为 `H750XBH6 ExtFlash Debug`
4. `Main` 标签：
   - C/C++ Application: `Debug_ExtFlash/H750XBH6.elf`
5. `Debugger` 标签：
   - **重要**: 需要配置 External Loader for QSPI Flash
   - External Loader: 选择 `W25Q256` 或对应型号

### 6. 编译流程

#### 编译 Bootloader
```
Project → Build Configurations → Set Active → Debug_Bootloader
Project → Build Project
```

输出文件：
- `Debug_Bootloader/H750XBH6.elf`
- `Debug_Bootloader/H750XBH6.hex`
- `Debug_Bootloader/H750XBH6.bin`

#### 编译应用程序
```
Project → Build Configurations → Set Active → Debug_ExtFlash
Project → Build Project
```

输出文件：
- `Debug_ExtFlash/H750XBH6.elf`
- `Debug_ExtFlash/H750XBH6.hex`
- `Debug_ExtFlash/H750XBH6.bin`

### 7. 烧录流程

#### 方法 A: 使用 STM32CubeIDE 直接烧录

**步骤 1**: 烧录 Bootloader
1. 选择 `Debug_Bootloader` 配置
2. 点击 `Run` → `Debug` 或 Flash 按钮
3. Bootloader 将被烧录到内部 Flash (0x08000000)

**步骤 2**: 配置外部 Flash 烧录器
1. 在 `Debug Configurations` 中选择外部 Flash 配置
2. `Debugger` → `External Loaders` 标签
3. 勾选 `W25Q256` 或对应的 QSPI Flash loader

**步骤 3**: 烧录应用程序
1. 选择 `Debug_ExtFlash` 配置
2. 点击 `Run` → `Debug` 或 Flash 按钮
3. 应用程序将被烧录到外部 Flash (0x90000000)

#### 方法 B: 使用 STM32CubeProgrammer

**烧录 Bootloader**:
```
STM32_Programmer_CLI -c port=SWD -w Debug_Bootloader/H750XBH6.hex -v -s
```

**烧录应用程序到外部 Flash**:
```
# 首先加载外部 Flash loader
STM32_Programmer_CLI -c port=SWD -el "C:\Path\To\ExternalLoader\W25Q256.stldr"

# 然后烧录应用程序
STM32_Programmer_CLI -c port=SWD -w Debug_ExtFlash/H750XBH6.hex 0x90000000 -v
```

### 8. 验证配置

#### 8.1 检查 Bootloader 大小

编译后检查输出：
```
text    data     bss     dec     hex filename
24576   1024    4096   29696    7400 H750XBH6.elf
```

确保 `text + data < 64KB (65536 bytes)`

#### 8.2 检查应用程序链接

查看 `.map` 文件，确认：
- 代码段在 `0x90000000` 开始
- 数据段在 RAM 区域 (`0x24000000` 等)

#### 8.3 串口输出验证

连接 UART1 (115200, 8N1)，应该看到：

**Bootloader 输出**:
```
========================================
STM32H750 Bootloader Starting...
========================================
QSPI Flash W25Q256 initialized successfully
QSPI Flash ID: 0xEF4019
...
Jumping to application at address: 0x90000XXX
```

**应用程序输出**:
```
Starting main function initialization...
UART1 initialized at 115200 baud
...
```

### 9. 常见问题解决

#### 问题 1: 链接脚本找不到

**症状**: `cannot find -lSTM32H750XBHX_BOOTLOADER.ld`

**解决**: 使用相对路径或绝对路径：
```
../STM32H750XBHX_BOOTLOADER.ld
```

#### 问题 2: 编译时提示符号未定义

**症状**: `undefined reference to 'Bootloader_Init'`

**解决**: 
1. 确认 `bootloader.c` 已添加到工程
2. 刷新工程: 右键项目 → `Refresh`
3. 清理并重新编译: `Project` → `Clean...`

#### 问题 3: 外部 Flash 无法烧录

**症状**: Programming failed, cannot access external memory

**解决**:
1. 先烧录 Bootloader
2. 在 STM32CubeProgrammer 中配置正确的 External Loader
3. 确认硬件连接正常

### 10. Makefile 配置（如果使用命令行编译）

在 `Makefile` 中添加：

```makefile
# Bootloader 配置
ifeq ($(BOOTLOADER),1)
C_DEFS += -DUSE_BOOTLOADER
LDSCRIPT = STM32H750XBHX_BOOTLOADER.ld
else
LDSCRIPT = STM32H750XBHX_EXTFLASH.ld
endif
```

编译 Bootloader:
```bash
make BOOTLOADER=1 clean all
```

编译应用程序:
```bash
make BOOTLOADER=0 clean all
```

### 11. 自动化脚本（可选）

创建 `build_all.sh` (Linux/Mac) 或 `build_all.bat` (Windows):

**build_all.bat**:
```batch
@echo off
echo Building Bootloader...
arm-none-eabi-gcc -DUSE_BOOTLOADER ... -T STM32H750XBHX_BOOTLOADER.ld

echo Building Application...
arm-none-eabi-gcc ... -T STM32H750XBHX_EXTFLASH.ld

echo Done!
```

### 12. 版本控制建议

在 `.gitignore` 中添加：
```
Debug_Bootloader/
Debug_ExtFlash/
*.elf
*.hex
*.bin
*.map
*.list
```

但保留链接脚本：
```
!*.ld
```

---

## 完整配置检查清单

- [ ] 创建了 `Debug_Bootloader` 构建配置
- [ ] 创建了 `Debug_ExtFlash` 构建配置
- [ ] Bootloader 配置中添加了 `USE_BOOTLOADER` 宏
- [ ] Bootloader 配置使用 `STM32H750XBHX_BOOTLOADER.ld`
- [ ] 应用程序配置使用 `STM32H750XBHX_EXTFLASH.ld`
- [ ] 应用程序中添加了向量表重定位代码
- [ ] `bootloader.c` 和 `qspi_w25q256.c` 已包含在工程中
- [ ] 包含路径中添加了 `Drivers/Flash_driver/Inc`
- [ ] 配置了外部 Flash 调试器 loader
- [ ] 测试了编译和烧录流程
- [ ] 验证了串口输出

完成以上步骤后，您的 STM32H750 Bootloader 工程就配置完成了！
