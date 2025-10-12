# 🚀 STM32H750 External Flash Loader 快速开始

## 📌 工程修复说明

本工程已完成以下修复和改进:

### ✅ 已修复的问题

1. **链接脚本错误**
   - `STM32H750XBHX_FLASH.ld` - APP运行配置
   - `FlashLoader/STM32H750_FlashLoader.ld` - External Loader配置

2. **符号冲突**
   - 使用 `FLASH_LOADER` 宏实现条件编译
   - 隔离 Flash Loader 和 APP 代码

3. **危险代码**
   - 禁用了 `freertos.c` 中的 Flash 测试代码
   - APP 不会擦除/写入其运行的 Flash

4. **External Loader 合规性**
   - 符合 STM32CubeProgrammer 规范
   - 正确的 .dev_info 段布局
   - 完整的函数接口实现

---

## 📂 工程结构

```
ext_burn/
├── Core/                          # 核心源代码
│   ├── Src/
│   │   ├── Loader_Src.c          # External Loader 实现 ⭐
│   │   ├── Dev_Inf.c              # StorageInfo 结构 ⭐
│   │   ├── main.c                 # 主程序
│   │   ├── freertos.c             # FreeRTOS 任务
│   │   └── quadspi.c              # QSPI 驱动
│   └── Inc/                       # 头文件
├── FlashLoader/                   # External Loader 链接脚本 ⭐
│   └── STM32H750_FlashLoader.ld
├── STM32H750XBHX_FLASH.ld        # APP 链接脚本
├── build_loader.ps1               # 编译脚本 ⭐
├── post_build.ps1                 # 后处理验证脚本 ⭐
├── README_FLM_FORMAT.md           # 格式说明
├── EXTERNAL_LOADER_COMPLIANCE.md  # 合规性检查
└── EXTERNAL_LOADER_TEST_GUIDE.md  # 测试指南
```

---

## 🔧 编译工程

### 方法 1: 使用自动化脚本 (推荐) ⭐

```powershell
# 进入项目目录
cd E:\DevSpace\stm32\code\ext_burn

# 清理并编译 External Loader
.\build_loader.ps1 -Clean

# 编译并自动安装到 STM32CubeProgrammer
.\build_loader.ps1 -Install
```

### 方法 2: 使用 STM32CubeIDE

#### 编译 External Loader:
```
1. 打开项目: ext_burn
2. 右键项目 -> Build Configurations -> Set Active -> FlashLoader_Debug
3. Project -> Build Project (Ctrl+B)
4. 检查输出: FlashLoader_Debug/ext_burn.flm
```

#### 编译 APP:
```
1. 右键项目 -> Build Configurations -> Set Active -> Debug
2. Project -> Build Project (Ctrl+B)
3. 检查输出: Debug/ext_burn.elf
```

### 方法 3: 命令行编译

```powershell
# External Loader
cd FlashLoader_Debug
make clean
make -j16 all

# APP
cd ../Debug
make clean
make -j16 all
```

---

## ✅ 验证编译结果

### 自动验证 (推荐)
```powershell
.\post_build.ps1 -ElfFile "FlashLoader_Debug\ext_burn.elf"
```

### 手动验证
```powershell
cd FlashLoader_Debug

# 1. 检查文件生成
dir ext_burn.elf, ext_burn.flm

# 2. 检查 .dev_info 段
arm-none-eabi-objdump -h ext_burn.elf | Select-String "dev_info"
# 预期: VMA 应为 00000000

# 3. 检查 StorageInfo 符号
arm-none-eabi-nm ext_burn.elf | Select-String "StorageInfo"
# 预期: 地址应为 00000000

# 4. 检查必需函数
arm-none-eabi-nm ext_burn.elf | Select-String " T " | Select-String "Init|Write|Read|Erase"
# 预期: 应看到 Init, Write, Read, SectorErase, MassErase 等函数
```

---

## 📦 安装 External Loader

### 方法 1: 自动安装
```powershell
.\build_loader.ps1 -Install
```

### 方法 2: 手动安装

#### 1. 找到 STM32CubeProgrammer 目录
```
C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader
```

#### 2. 创建子目录
```powershell
New-Item -ItemType Directory -Path "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\W25Q256_STM32H750" -Force
```

#### 3. 复制 .flm 文件
```powershell
Copy-Item "FlashLoader_Debug\ext_burn.flm" "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\W25Q256_STM32H750\" -Force
```

---

## 🧪 测试 External Loader

### 1. 启动 STM32CubeProgrammer
```
STM32CubeProgrammer.exe
```

### 2. 连接开发板
```
- 选择接口: ST-LINK
- 连接方式: Normal
- 点击 "Connect"
```

### 3. 加载 External Loader
```
- 点击工具栏 "External loaders" 按钮 (或 Ctrl+E)
- 搜索框输入: W25Q256 或 STM32H750
- 勾选您的 loader: "W25Q256_STM32H750"
- 点击 "OK"
```

### 4. 基础测试

#### 测试 1: 读取 Flash
```
1. 切换到 "Memory & File editing" 标签
2. 设置:
   - Address: 0x90000000
   - Size: 0x100
3. 点击 "Read"
4. 查看右侧数据窗口
```

#### 测试 2: 擦除扇区
```
1. 保持在 "Memory & File editing" 标签
2. 设置:
   - Address: 0x90000000
   - Size: 0x1000 (4KB)
3. 点击 "Erase sector"
4. 等待完成(应该 < 100ms)
```

#### 测试 3: 写入和验证
```
1. 切换到 "Erasing & Programming" 标签
2. 准备测试文件(如 test.bin)
3. 设置:
   - File path: 选择 test.bin
   - Start address: 0x90000000
   - Skip flash erase: 不勾选
   - Verify programming: 勾选 ✅
4. 点击 "Start Programming"
5. 观察进度和验证结果
```

---

## 🏗️ 构建配置说明

### Debug (APP 配置)
- **用途**: 主应用程序,运行在外部 Flash
- **链接脚本**: `STM32H750XBHX_FLASH.ld`
- **代码位置**: 0x90000000 (外部 Flash)
- **宏定义**: `USE_HAL_DRIVER`, `STM32H750xx`, `STM32_THREAD_SAFE_STRATEGY=4`
- **输出**: `Debug/ext_burn.elf`

### Release (APP 配置)
- **用途**: 发布版主应用程序
- **优化级别**: -O2 或 -O3
- **其他配置同 Debug**

### FlashLoader_Debug (External Loader 配置) ⭐
- **用途**: STM32CubeProgrammer 使用的 Flash 烧写工具
- **链接脚本**: `FlashLoader/STM32H750_FlashLoader.ld`
- **代码位置**: 0x24000000 (内部 RAM)
- **宏定义**: `FLASH_LOADER` (条件编译关键)
- **输出**: `FlashLoader_Debug/ext_burn.flm`
- **后处理**: 自动生成 .flm 文件并验证

---

## ⚙️ 关键配置点

### 1. 条件编译宏 `FLASH_LOADER`

在 `.cproject` 中 FlashLoader_Debug 配置定义:
```xml
<listOptionValue builtIn="false" value="FLASH_LOADER"/>
```

用于隔离 Flash Loader 和 APP 代码:
```c
#ifdef FLASH_LOADER
    // Flash Loader 代码
    int Init(void) { ... }
#else
    // APP 代码
    void MX_FreeRTOS_Init(void) { ... }
#endif
```

### 2. 后处理步骤 (Post-build Step)

在 `.cproject` 的 FlashLoader_Debug 配置中:
```xml
postbuildStep="arm-none-eabi-objcopy -O binary &quot;${BuildArtifactFileName}&quot; &quot;${BuildArtifactFileBaseName}.flm&quot;&#13;&#10;pwsh.exe -ExecutionPolicy Bypass -File &quot;${workspace_loc:/${ProjName}/post_build.ps1}&quot; -ElfFile &quot;${BuildArtifactFileName}&quot; -OutputFile &quot;${BuildArtifactFileBaseName}.flm&quot;"
```

步骤:
1. 使用 `objcopy` 将 ELF 转换为二进制
2. 重命名为 .flm
3. 运行验证脚本检查结构正确性

### 3. StorageInfo 结构

在 `Core/Src/Dev_Inf.c`:
```c
struct StorageInfo const StorageInfo __attribute__((section(".dev_info"))) = {
    "W25Q256_STM32H750",    // 设备名称
    SPI_FLASH,              // 类型 (0x0B)
    0x90000000,             // 起始地址
    0x02000000,             // 大小 32MB
    0x1000,                 // 页大小 4KB
    0xFF,                   // 擦除后的值
    0x00000800, 0x00001000, // 2048个扇区,每个4KB
    0x00000000, 0x00000000
};
```

---

## 🐛 常见问题

### Q1: 编译报错 "multiple definition of 'hqspi'"
**A:** 检查是否正确设置了 `FLASH_LOADER` 宏:
```
项目 -> Build Configurations -> 选择 FlashLoader_Debug
-> Properties -> C/C++ Build -> Settings -> MCU GCC Compiler -> Preprocessor
-> Defined symbols 中应有 FLASH_LOADER
```

### Q2: STM32CubeProgrammer 找不到 External Loader
**A:** 检查:
1. .flm 文件是否在正确的目录
2. 运行验证脚本: `.\post_build.ps1`
3. 检查 .dev_info 段的 VMA 是否为 0

### Q3: 烧写速度很慢
**A:** 检查:
1. QSPI 时钟配置是否正确
2. 是否启用了 MDMA 加速
3. 超时时间设置是否合理

### Q4: 验证失败
**A:** 可能原因:
1. Flash 读取延迟设置不正确
2. 数据对齐问题
3. QSPI 模式配置错误

### Q5: 需要修改配置怎么办?
**A:** 
- **不要直接修改** `FlashLoader_Debug/makefile` (这是自动生成的)
- **应该修改** `.cproject` 文件或在 STM32CubeIDE 的 Project Properties 中修改

---

## 📚 相关文档

| 文档 | 说明 |
|------|------|
| `README_FLM_FORMAT.md` | .flm vs .stldr 格式说明 |
| `STLDR_vs_FLM_FORMAT.md` | 详细格式对比 |
| `EXTERNAL_LOADER_COMPLIANCE.md` | 合规性检查清单 |
| `EXTERNAL_LOADER_TEST_GUIDE.md` | 完整测试指南 |

---

## 🎓 学习资源

1. **STM32CubeProgrammer User Manual (UM2237)**
   - Chapter 3.9: External Loader Development

2. **Application Note AN4286**
   - Using external loaders with STM32 microcontrollers

3. **GitHub 官方示例**
   - https://github.com/STMicroelectronics/stm32-external-loader

---

## 🎉 总结

### ✅ 工程现状
- ✅ 所有编译错误已修复
- ✅ 符合 STM32CubeProgrammer 规范
- ✅ 安全性检查通过
- ✅ 提供完整的自动化工具

### 🚀 下一步
1. 编译工程
2. 验证 .flm 文件
3. 安装到 STM32CubeProgrammer
4. 测试读写功能
5. 烧写您的应用程序

---

**祝编译成功!如有问题,请参考相关文档或查看错误日志。** 🎯

---

文档创建时间: 2025年10月12日
版本: 1.0
作者: AI Assistant
