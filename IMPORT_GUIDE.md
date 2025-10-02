# STM32CubeIDE 工程导入和配置指南

## 问题说明

如果您遇到以下问题：
- 双击 `.cproject` 文件无法正常导入工程
- 工程名称不匹配
- 缺少 Bootloader 和 ExtFlash 构建配置

本指南将帮您解决这些问题。

## ✅ 已修复的问题

1. **项目名称不匹配**：已从 `ext_burn` 改为 `H750XBH`
2. **构建路径引用**：已更新所有项目名称引用

## 📥 正确导入工程的步骤

### 步骤 1：启动 STM32CubeIDE

1. 启动 STM32CubeIDE
2. 选择一个工作空间（Workspace），建议不要选择项目所在目录

### 步骤 2：导入现有项目

1. **File** → **Import...**
2. 展开 **General**
3. 选择 **Existing Projects into Workspace**
4. 点击 **Next**

### 步骤 3：选择项目

1. 在 **Select root directory** 中点击 **Browse...**
2. 浏览到：`E:\DevSpace\stm32\code\H750XBH`
3. 确保看到项目 `H750XBH` 已被勾选
4. **重要**：**不要勾选** "Copy projects into workspace"
5. 点击 **Finish**

### 步骤 4：等待索引完成

- 导入后，STM32CubeIDE 会自动索引项目文件
- 等待右下角的进度条完成
- 这可能需要几分钟时间

## 🔧 创建 Bootloader 和应用程序构建配置

导入成功后，需要创建两个新的构建配置：

### 创建 Debug_Bootloader 配置

1. **右键点击项目** `H750XBH` → **Build Configurations** → **Manage...**
2. 点击 **New...** 按钮
3. 填写配置信息：
   - **Name**: `Debug_Bootloader`
   - **Copy settings from**: 选择 `Debug`
   - **Description**: Bootloader configuration for internal flash
4. 点击 **OK**

### 创建 Debug_ExtFlash 配置

1. 再次点击 **New...** 按钮
2. 填写配置信息：
   - **Name**: `Debug_ExtFlash`
   - **Copy settings from**: 选择 `Debug`
   - **Description**: Application configuration for external QSPI flash
3. 点击 **OK**
4. 点击 **OK** 关闭配置管理窗口

## ⚙️ 配置 Bootloader 构建

### 1. 选择 Bootloader 配置

- **Project** → **Build Configurations** → **Set Active** → **Debug_Bootloader**

### 2. 打开项目属性

- **右键项目** → **Properties**

### 3. 添加预处理器宏

1. 导航到：**C/C++ Build** → **Settings**
2. 展开：**MCU GCC Compiler** → **Preprocessor**
3. 在 **Define symbols (-D)** 列表中点击 **Add** 图标（绿色加号）
4. 输入：`USE_BOOTLOADER`
5. 点击 **OK**

### 4. 修改链接脚本

1. 在同一个 **Settings** 窗口中
2. 展开：**MCU GCC Linker** → **General**
3. 找到 **Linker Script (-T)** 字段
4. 将值从 `${workspace_loc:/${ProjName}/STM32H750XBHX_FLASH.ld}` 改为：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_BOOTLOADER.ld}
   ```
5. 点击 **Apply**

### 5. 优化代码大小（可选但推荐）

1. 展开：**MCU GCC Compiler** → **Optimization**
2. **Optimization level** 选择：`Optimize for size (-Os)`
3. 点击 **Apply and Close**

## ⚙️ 配置 ExtFlash 应用程序构建

### 1. 选择 ExtFlash 配置

- **Project** → **Build Configurations** → **Set Active** → **Debug_ExtFlash**

### 2. 打开项目属性

- **右键项目** → **Properties**

### 3. 确认预处理器宏

1. 导航到：**C/C++ Build** → **Settings**
2. 展开：**MCU GCC Compiler** → **Preprocessor**
3. 在 **Define symbols (-D)** 列表中：
   - 确保有：`DEBUG`, `USE_HAL_DRIVER`, `STM32H750xx`, 等
   - **确保没有**：`USE_BOOTLOADER`（如果有，删除它）

### 4. 修改链接脚本

1. 展开：**MCU GCC Linker** → **General**
2. 找到 **Linker Script (-T)** 字段
3. 将值改为：
   ```
   ${workspace_loc:/${ProjName}/STM32H750XBHX_EXTFLASH.ld}
   ```
4. 点击 **Apply and Close**

## 🔍 验证配置

### 检查 Bootloader 配置

1. 切换到 **Debug_Bootloader** 配置
2. 右键项目 → **Properties** → **C/C++ Build** → **Settings**
3. 确认：
   - ✅ 预处理器宏中有 `USE_BOOTLOADER`
   - ✅ 链接脚本是 `STM32H750XBHX_BOOTLOADER.ld`

### 检查 ExtFlash 配置

1. 切换到 **Debug_ExtFlash** 配置
2. 右键项目 → **Properties** → **C/C++ Build** → **Settings**
3. 确认：
   - ✅ 预处理器宏中**没有** `USE_BOOTLOADER`
   - ✅ 链接脚本是 `STM32H750XBHX_EXTFLASH.ld`

## 🏗️ 首次编译

### 编译 Bootloader

1. **Project** → **Build Configurations** → **Set Active** → **Debug_Bootloader**
2. **Project** → **Clean...** → 选择项目 → **Clean**
3. **Project** → **Build Project** 或按 **Ctrl+B**
4. 检查 Console 输出，确保编译成功
5. 检查编译大小，确保 **text + data < 64KB**

### 编译应用程序

1. **Project** → **Build Configurations** → **Set Active** → **Debug_ExtFlash**
2. **Project** → **Clean...**
3. **Project** → **Build Project**
4. 检查编译成功

## ❗ 常见问题和解决方法

### 问题 1：找不到链接脚本

**错误信息**：`cannot find -lSTM32H750XBHX_BOOTLOADER.ld`

**解决方法**：
1. 确认链接脚本文件存在于项目根目录
2. 刷新项目：右键项目 → **Refresh** (F5)
3. 清理并重新编译

### 问题 2：无法切换构建配置

**症状**：只能看到 Debug 和 Release 配置

**解决方法**：
1. 关闭 STM32CubeIDE
2. 删除项目目录下的 `.settings` 文件夹（如果存在）
3. 重新导入项目
4. 重新创建构建配置

### 问题 3：编译错误 - 找不到头文件

**错误信息**：`fatal error: bootloader.h: No such file or directory`

**解决方法**：
1. 右键项目 → **Properties** → **C/C++ Build** → **Settings**
2. **MCU GCC Compiler** → **Include paths**
3. 添加：`../Drivers/Flash_driver/Inc`
4. 点击 **Apply and Close**
5. 清理并重新编译

### 问题 4：链接错误 - 找不到符号

**错误信息**：`undefined reference to 'Bootloader_Init'`

**解决方法**：
1. 确认文件已添加到项目：
   - `Drivers/Flash_driver/Src/bootloader.c`
   - `Drivers/Flash_driver/Src/qspi_w25q256.c`
2. 刷新项目 (F5)
3. 清理并重新编译

### 问题 5：Bootloader 太大

**错误信息**：Section .text exceeds available size

**解决方法**：
1. 右键项目 → **Properties** → **C/C++ Build** → **Settings**
2. **MCU GCC Compiler** → **Optimization**
3. 选择 **Optimize for size (-Os)**
4. 在 **Bootloader 配置**中：
   - 可以考虑移除不必要的调试输出
   - 移除 FreeRTOS（Bootloader 不需要）

## 📋 配置检查清单

在开始编译前，请确认：

### Bootloader 配置 (Debug_Bootloader)
- [ ] 预处理器宏包含 `USE_BOOTLOADER`
- [ ] 链接脚本设置为 `STM32H750XBHX_BOOTLOADER.ld`
- [ ] 优化级别设置为 `-Os` (可选)
- [ ] 包含路径包含 `../Drivers/Flash_driver/Inc`

### 应用程序配置 (Debug_ExtFlash)
- [ ] 预处理器宏**不包含** `USE_BOOTLOADER`
- [ ] 链接脚本设置为 `STM32H750XBHX_EXTFLASH.ld`
- [ ] 包含路径包含 `../Drivers/Flash_driver/Inc`
- [ ] 在代码中添加了向量表重定位 `SCB->VTOR = 0x90000000;`

### 源文件
- [ ] `Drivers/Flash_driver/Src/bootloader.c` 存在
- [ ] `Drivers/Flash_driver/Src/qspi_w25q256.c` 存在
- [ ] `Drivers/Flash_driver/Inc/bootloader.h` 存在
- [ ] `Drivers/Flash_driver/Inc/qspi_w25q256.h` 存在

### 链接脚本
- [ ] `STM32H750XBHX_BOOTLOADER.ld` 存在于项目根目录
- [ ] `STM32H750XBHX_EXTFLASH.ld` 存在于项目根目录

## 🎯 下一步

配置完成后：
1. 参考 `BOOTLOADER_README.md` 了解如何烧录固件
2. 参考 `QUICK_REFERENCE.md` 快速查看关键配置
3. 参考 `ARCHITECTURE_DIAGRAM.md` 理解系统架构

## 💡 提示

- 首次导入和编译可能需要较长时间
- 如果遇到问题，尝试清理项目并重新编译
- 确保 STM32CubeIDE 版本至少为 1.8.0 或更高
- 保持工具链（GCC）版本更新

## 📞 需要帮助？

如果按照本指南操作后仍有问题：
1. 检查 Console 输出中的具体错误信息
2. 查看 Problems 视图中的错误列表
3. 确认所有必需文件都存在
4. 尝试重新导入项目

---

**重要提示**：每次修改构建配置后，建议执行 **Clean** 操作，然后重新编译，以确保所有更改生效。
