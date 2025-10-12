# STM32 External Loader 后处理脚本
# 用于从 ELF 文件生成 .flm 格式
# 适用于 STM32CubeProgrammer

param(
    [Parameter(Mandatory=$false)]
    [string]$ElfFile = "ext_burn.elf",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputFile = "ext_burn.flm"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  STM32 External Loader Post-Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查 ELF 文件是否存在
if (-not (Test-Path $ElfFile)) {
    Write-Host "❌ 错误: 找不到 ELF 文件 '$ElfFile'" -ForegroundColor Red
    exit 1
}

Write-Host "✅ 找到 ELF 文件: $ElfFile" -ForegroundColor Green
$elfSize = (Get-Item $ElfFile).Length
Write-Host "   文件大小: $($elfSize / 1KB) KB" -ForegroundColor Gray

# 步骤 1: 验证 .dev_info 段
Write-Host ""
Write-Host "📋 步骤 1: 验证 .dev_info 段..." -ForegroundColor Yellow

$devInfoCheck = & arm-none-eabi-objdump -h $ElfFile | Select-String ".dev_info"
if ($devInfoCheck) {
    Write-Host "✅ .dev_info 段存在" -ForegroundColor Green
    Write-Host "   $devInfoCheck" -ForegroundColor Gray
    
    # 检查 VMA 是否为 0
    if ($devInfoCheck -match "00000000") {
        Write-Host "✅ VMA 地址正确 (0x00000000)" -ForegroundColor Green
    } else {
        Write-Host "⚠️  警告: VMA 地址可能不正确!" -ForegroundColor Yellow
    }
} else {
    Write-Host "❌ 错误: 找不到 .dev_info 段!" -ForegroundColor Red
    Write-Host "   External Loader 必须包含 .dev_info 段" -ForegroundColor Red
    exit 1
}

# 步骤 2: 验证 StorageInfo 符号
Write-Host ""
Write-Host "📋 步骤 2: 验证 StorageInfo 符号..." -ForegroundColor Yellow

$storageInfoCheck = & arm-none-eabi-nm $ElfFile | Select-String "StorageInfo"
if ($storageInfoCheck) {
    Write-Host "✅ StorageInfo 符号存在" -ForegroundColor Green
    Write-Host "   $storageInfoCheck" -ForegroundColor Gray
    
    # 检查地址是否为 0
    if ($storageInfoCheck -match "^00000000") {
        Write-Host "✅ StorageInfo 地址正确 (0x00000000)" -ForegroundColor Green
    } else {
        Write-Host "⚠️  警告: StorageInfo 地址可能不正确!" -ForegroundColor Yellow
    }
} else {
    Write-Host "❌ 错误: 找不到 StorageInfo 符号!" -ForegroundColor Red
    exit 1
}

# 步骤 3: 验证必需的函数
Write-Host ""
Write-Host "📋 步骤 3: 验证必需的函数..." -ForegroundColor Yellow

$requiredFunctions = @("Init", "Write", "Read", "SectorErase", "MassErase")
$allFunctionsFound = $true

foreach ($func in $requiredFunctions) {
    $funcCheck = & arm-none-eabi-nm $ElfFile | Select-String -Pattern "\s+T\s+$func$"
    if ($funcCheck) {
        Write-Host "   ✅ $func" -ForegroundColor Green
    } else {
        Write-Host "   ❌ $func (未找到)" -ForegroundColor Red
        $allFunctionsFound = $false
    }
}

if (-not $allFunctionsFound) {
    Write-Host ""
    Write-Host "❌ 错误: 缺少必需的函数!" -ForegroundColor Red
    exit 1
}

# 步骤 4: 生成二进制文件
Write-Host ""
Write-Host "📋 步骤 4: 生成二进制文件..." -ForegroundColor Yellow

$binFile = [System.IO.Path]::ChangeExtension($OutputFile, ".bin")

try {
    # 使用 objcopy 转换为二进制
    # 不移除任何段,保持完整的内存布局
    & arm-none-eabi-objcopy -O binary $ElfFile $binFile
    
    if (Test-Path $binFile) {
        $binSize = (Get-Item $binFile).Length
        Write-Host "✅ 二进制文件生成成功: $binFile" -ForegroundColor Green
        Write-Host "   文件大小: $($binSize / 1KB) KB" -ForegroundColor Gray
    } else {
        Write-Host "❌ 错误: 二进制文件生成失败!" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ 错误: objcopy 执行失败!" -ForegroundColor Red
    Write-Host "   $_" -ForegroundColor Red
    exit 1
}

# 步骤 5: 重命名为 .flm
Write-Host ""
Write-Host "📋 步骤 5: 生成 .flm 文件..." -ForegroundColor Yellow

try {
    Copy-Item $binFile $OutputFile -Force
    
    if (Test-Path $OutputFile) {
        $flmSize = (Get-Item $OutputFile).Length
        Write-Host "✅ FLM 文件生成成功: $OutputFile" -ForegroundColor Green
        Write-Host "   文件大小: $($flmSize / 1KB) KB" -ForegroundColor Gray
    } else {
        Write-Host "❌ 错误: FLM 文件生成失败!" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ 错误: 文件复制失败!" -ForegroundColor Red
    Write-Host "   $_" -ForegroundColor Red
    exit 1
}

# 步骤 6: 生成反汇编文件 (可选,用于调试)
Write-Host ""
Write-Host "📋 步骤 6: 生成反汇编文件 (调试用)..." -ForegroundColor Yellow

$lstFile = [System.IO.Path]::ChangeExtension($OutputFile, ".lst")

try {
    & arm-none-eabi-objdump -d -S $ElfFile > $lstFile
    
    if (Test-Path $lstFile) {
        Write-Host "✅ 反汇编文件生成成功: $lstFile" -ForegroundColor Green
    }
} catch {
    Write-Host "⚠️  警告: 反汇编文件生成失败" -ForegroundColor Yellow
}

# 总结
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  构建完成!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor White
Write-Host "  - $ElfFile" -ForegroundColor Gray
Write-Host "  - $binFile" -ForegroundColor Gray
Write-Host "  - $OutputFile" -ForegroundColor Green -NoNewline
Write-Host " ← 这个文件用于 STM32CubeProgrammer" -ForegroundColor Yellow

Write-Host ""
Write-Host "下一步:" -ForegroundColor White
Write-Host "  1. 将 $OutputFile 复制到 STM32CubeProgrammer 的 ExternalLoader 目录" -ForegroundColor Gray
Write-Host "  2. 启动 STM32CubeProgrammer" -ForegroundColor Gray
Write-Host "  3. 连接目标板" -ForegroundColor Gray
Write-Host "  4. 在 External Loaders 列表中选择您的 loader" -ForegroundColor Gray
Write-Host "  5. 测试读写操作" -ForegroundColor Gray
Write-Host ""

# 可选: 显示文件内容摘要
if ($env:VERBOSE -eq "1") {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  详细信息 (VERBOSE模式)" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    Write-Host ""
    Write-Host "段信息:" -ForegroundColor Yellow
    & arm-none-eabi-objdump -h $ElfFile
    
    Write-Host ""
    Write-Host "符号表 (前50个):" -ForegroundColor Yellow
    & arm-none-eabi-nm -n $ElfFile | Select-Object -First 50
}

Write-Host "✅ 所有步骤完成!" -ForegroundColor Green
exit 0
