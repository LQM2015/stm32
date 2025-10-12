# MPU 配置问题分析和解决方案

## 🔴 问题发现

从 bootloader 跳转到 APP 后,APP 无法运行,没有任何输出。

### 根本原因:MPU 配置错误

在 `main.c` 的 `MPU_Config()` 函数中:

```c
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  
  HAL_MPU_Disable();
  
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;          // 整个地址空间
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;        // ❌ 致命!
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;   // ❌ 致命!
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 问题分析

这个 MPU 配置存在两个致命问题:

1. **`AccessPermission = MPU_REGION_NO_ACCESS`**
   - 禁止了对整个 4GB 地址空间的读写访问
   - 包括外部 Flash (0x90000000)、RAM、外设等

2. **`DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE`**
   - 禁止了在整个 4GB 地址空间执行代码
   - APP 无法从外部 Flash (0x90000000) 执行

### 执行时序

```
[APP 启动]
1. Bootloader 跳转到 APP Reset_Handler ✅
2. Reset_Handler 调用 SystemInit() ✅
3. SystemInit() 设置 VTOR = 0x90000000 ✅
4. Reset_Handler 调用 main() ✅
5. main() 调用 MPU_Config() 
6. MPU 启用,设置为 NO_ACCESS + DISABLE_EXEC ❌
7. CPU 尝试从 0x90000000 执行下一条指令
8. MPU 阻止访问! 💥
9. HardFault 异常
10. CPU 从 VTOR+0x0C 读取 HardFault 处理函数地址
11. 跳转到 HardFault_Handler
12. HardFault_Handler 进入死循环 (没有 UART 输出)
```

### 为什么没有看到 HardFault 输出?

- HardFault_Handler 通常是一个简单的死循环
- 没有调用 DEBUG_ERROR 或其他输出函数
- 所以表现为"程序静默崩溃"

## ✅ 解决方案

### 方案 1: 临时禁用 MPU (测试用)

在 `main.c` 中注释掉 MPU_Config():

```c
/* MPU Configuration */
// MPU_Config();  // 临时禁用测试
```

**测试步骤**:
1. 重新编译 APP
2. 烧录到外部 Flash
3. 重启观察是否能看到 APP 输出

如果能看到输出,说明 MPU 确实是问题根源。

### 方案 2: 正确配置 MPU (推荐)

需要为不同的内存区域配置不同的 MPU 区域:

```c
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  
  /* 禁用 MPU */
  HAL_MPU_Disable();
  
  /* Region 0: 外部 QSPI Flash - 可读可执行,可缓存 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;     // ✅ 允许访问
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;   // ✅ 允许执行
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;            // ✅ 启用缓存提高性能
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Region 1: 内部 SRAM - 可读可写可执行,可缓存 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x20000000;  // DTCM RAM
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Region 2: AXI SRAM (D1) - 可读可写可执行,可缓存 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Region 3: 外设区域 - 可读可写,不可执行,不可缓存 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0x40000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;  // 外设不可执行
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;        // 外设不可缓存
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* 启用 MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 方案 3: 完全不使用 MPU (最简单)

如果不需要 MPU 的保护功能,可以完全删除 MPU_Config() 调用:

```c
int main(void)
{
  /* MPU Configuration - 不需要就不要配置 */
  // MPU_Config();
  
  /* Enable the CPU Cache */
  SCB_EnableICache();
  SCB_EnableDCache();
  
  // ... 其他初始化
}
```

## 📊 MPU 配置对比

| 配置项 | 原始(错误) | 推荐 | 说明 |
|--------|----------|------|------|
| 外部 Flash 访问权限 | NO_ACCESS | FULL_ACCESS | 必须允许读取 |
| 外部 Flash 执行权限 | DISABLE | ENABLE | 必须允许执行代码 |
| 外部 Flash 缓存 | NOT_CACHEABLE | CACHEABLE | 启用缓存提高性能 |
| RAM 访问权限 | NO_ACCESS | FULL_ACCESS | 必须允许读写 |
| RAM 执行权限 | DISABLE | ENABLE | 允许从 RAM 执行 |
| 外设访问权限 | NO_ACCESS | FULL_ACCESS | 必须允许访问 |
| 外设执行权限 | DISABLE | DISABLE | 外设区不应执行 |
| 外设缓存 | NOT_CACHEABLE | NOT_CACHEABLE | 外设不可缓存 |

## 🚀 测试步骤

### 第 1 步:禁用 MPU 测试

1. 修改 `main.c`:
   ```c
   // MPU_Config();  // 注释掉
   ```

2. 重新编译 APP

3. 烧录到外部 Flash (0x90000000)

4. 重启,观察 UART 输出

### 第 2 步:验证结果

如果看到以下输出,说明 MPU 确实是问题:
```
[INFO] Goodbye from bootloader...
[INFO] === APP SUCCESSFULLY STARTED ===       ← 成功!
[INFO] APP is running from external Flash
[INFO] Bootloader handoff successful!
```

### 第 3 步:实现正确的 MPU 配置

如果测试成功,可以选择:
- **选项 A**: 继续不使用 MPU (简单但安全性较低)
- **选项 B**: 实现方案 2 的正确 MPU 配置 (推荐)

## 💡 关键要点

1. **MPU 是强制访问控制机制**
   - 一旦启用,所有内存访问都必须符合 MPU 规则
   - 违反 MPU 规则会立即触发 MemManage 或 HardFault

2. **外部 Flash 必须正确配置**
   - 必须允许读取 (AccessPermission)
   - 必须允许执行 (DisableExec = ENABLE)
   - 建议启用缓存 (IsCacheable) 提高性能

3. **不同内存区域需要不同配置**
   - Flash: 可读可执行,可缓存
   - RAM: 可读可写可执行,可缓存
   - 外设: 可读可写不可执行,不可缓存

4. **调试技巧**
   - 遇到静默崩溃时,优先怀疑 MPU
   - 可以先禁用 MPU 测试
   - 使用调试器查看 MPU 配置寄存器

## 📝 总结

原始的 MPU 配置将整个 4GB 地址空间设置为 NO_ACCESS + DISABLE_EXEC,导致 APP 无法从外部 Flash 执行。

临时禁用 MPU 后,应该能成功看到 APP 的输出。

长期解决方案是为不同的内存区域配置正确的 MPU 规则。

---

**修复日期**: 2025-10-12  
**问题根源**: MPU 配置禁止外部 Flash 访问和执行  
**测试方案**: 临时禁用 MPU_Config()  
**长期方案**: 正确配置 MPU 区域  
