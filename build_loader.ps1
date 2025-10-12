# STM32 External Loader 编译脚本
# 用于快速编译和生成 External Loader

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "FlashLoader_Debug")]
    [string]$Configuration = "FlashLoader_Debug",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean,
    
    [Parameter(Mandatory=$false)]
    [switch]$Install
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  STM32 External Loader 编译" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "配置: $Configuration" -ForegroundColor Yellow
Write-Host ""

# 检查工作目录
$projectRoot = "E:\DevSpace\stm32\code\ext_burn"
if (-not (Test-Path $projectRoot)) {
    Write-Host "❌ 错误: 找不到项目目录 '$projectRoot'" -ForegroundColor Red
    exit 1
}

Set-Location $projectRoot

# 步骤 1: 清理 (如果需要)
if ($Clean) {
    Write-Host "🧹 清理旧的构建文件..." -ForegroundColor Yellow
    
    $buildDir = Join-Path $projectRoot $Configuration
    if (Test-Path $buildDir) {
        try {
            Set-Location $buildDir
            & make clean
            Write-Host "✅ 清理完成" -ForegroundColor Green
        } catch {
            Write-Host "⚠️  警告: 清理失败" -ForegroundColor Yellow
        }
        Set-Location $projectRoot
    }
    Write-Host ""
}

# 步骤 2: 编译
Write-Host "🔨 开始编译..." -ForegroundColor Yellow
Write-Host ""

$buildDir = Join-Path $projectRoot $Configuration
if (-not (Test-Path $buildDir)) {
    Write-Host "❌ 错误: 找不到构建目录 '$buildDir'" -ForegroundColor Red
    exit 1
}

Set-Location $buildDir

try {
    # 编译项目
    $startTime = Get-Date
    
    Write-Host "执行: make -j16 all" -ForegroundColor Gray
    Write-Host ""
    
    & make -j16 all 2>&1 | ForEach-Object {
        if ($_ -match "error:") {
            Write-Host $_ -ForegroundColor Red
        } elseif ($_ -match "warning:") {
            Write-Host $_ -ForegroundColor Yellow
        } elseif ($_ -match "Finished building") {
            Write-Host $_ -ForegroundColor Green
        } else {
            Write-Host $_ -ForegroundColor Gray
        }
    }
    
    $endTime = Get-Date
    $duration = $endTime - $startTime
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "❌ 编译失败!" -ForegroundColor Red
        Set-Location $projectRoot
        exit $LASTEXITCODE
    }
    
    Write-Host ""
    Write-Host "✅ 编译成功!" -ForegroundColor Green
    Write-Host "   用时: $($duration.TotalSeconds) 秒" -ForegroundColor Gray
    
} catch {
    Write-Host ""
    Write-Host "❌ 编译过程出错!" -ForegroundColor Red
    Write-Host "   $_" -ForegroundColor Red
    Set-Location $projectRoot
    exit 1
}

# 步骤 3: 检查生成的文件
Write-Host ""
Write-Host "📦 检查生成的文件..." -ForegroundColor Yellow

$elfFile = Join-Path $buildDir "ext_burn.elf"
$flmFile = Join-Path $buildDir "ext_burn.flm"

if (Test-Path $elfFile) {
    $elfSize = (Get-Item $elfFile).Length
    Write-Host "✅ ELF 文件: ext_burn.elf ($($elfSize / 1KB) KB)" -ForegroundColor Green
} else {
    Write-Host "❌ 错误: 找不到 ELF 文件!" -ForegroundColor Red
    Set-Location $projectRoot
    exit 1
}

if (Test-Path $flmFile) {
    $flmSize = (Get-Item $flmFile).Length
    Write-Host "✅ FLM 文件: ext_burn.flm ($($flmSize / 1KB) KB)" -ForegroundColor Green
} else {
    Write-Host "⚠️  警告: 找不到 FLM 文件,尝试生成..." -ForegroundColor Yellow
    
    try {
        & arm-none-eabi-objcopy -O binary $elfFile $flmFile
        if (Test-Path $flmFile) {
            $flmSize = (Get-Item $flmFile).Length
            Write-Host "✅ FLM 文件生成成功: ext_burn.flm ($($flmSize / 1KB) KB)" -ForegroundColor Green
        }
    } catch {
        Write-Host "❌ 错误: FLM 文件生成失败!" -ForegroundColor Red
        Set-Location $projectRoot
        exit 1
    }
}

# 步骤 4: 安装到 STM32CubeProgrammer (如果需要)
if ($Install) {
    Write-Host ""
    Write-Host "📥 安装到 STM32CubeProgrammer..." -ForegroundColor Yellow
    
    # 查找 STM32CubeProgrammer 安装路径
    $cubeProgrammerPaths = @(
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader",
        "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader"
    )
    
    $loaderDir = $null
    foreach ($path in $cubeProgrammerPaths) {
        if (Test-Path $path) {
            $loaderDir = $path
            break
        }
    }
    
    if ($loaderDir) {
        # 创建目标目录
        $targetDir = Join-Path $loaderDir "W25Q256_STM32H750"
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }
        
        # 复制 FLM 文件
        $targetFile = Join-Path $targetDir "ext_burn.flm"
        try {
            Copy-Item $flmFile $targetFile -Force
            Write-Host "✅ 已安装到: $targetFile" -ForegroundColor Green
        } catch {
            Write-Host "❌ 错误: 安装失败!" -ForegroundColor Red
            Write-Host "   $_" -ForegroundColor Red
        }
    } else {
        Write-Host "⚠️  警告: 找不到 STM32CubeProgrammer 安装目录" -ForegroundColor Yellow
        Write-Host "   请手动复制 FLM 文件到 ExternalLoader 目录" -ForegroundColor Gray
    }
}

# 总结
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  构建完成!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "生成的文件位于: $buildDir" -ForegroundColor White
Write-Host "  - ext_burn.elf" -ForegroundColor Gray
Write-Host "  - ext_burn.flm" -ForegroundColor Green -NoNewline
Write-Host " ← External Loader 文件" -ForegroundColor Yellow
Write-Host ""

if (-not $Install) {
    Write-Host "提示: 使用 " -NoNewline -ForegroundColor Gray
    Write-Host "-Install" -NoNewline -ForegroundColor Cyan
    Write-Host " 参数可自动安装到 STM32CubeProgrammer" -ForegroundColor Gray
    Write-Host ""
}

Write-Host "下一步:" -ForegroundColor White
Write-Host "  1. 将 ext_burn.flm 复制到 STM32CubeProgrammer 的 ExternalLoader 目录" -ForegroundColor Gray
Write-Host "  2. 启动 STM32CubeProgrammer 并连接开发板" -ForegroundColor Gray
Write-Host "  3. 在 External Loaders 中选择您的 loader" -ForegroundColor Gray
Write-Host "  4. 测试读写操作" -ForegroundColor Gray
Write-Host ""

Set-Location $projectRoot

Write-Host "✅ 完成!" -ForegroundColor Green
exit 0
