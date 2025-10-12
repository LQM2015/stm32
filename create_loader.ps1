# 生成 STM32CubeProgrammer 外部加载器 .stldr 文件 (ELF 格式)
# 
# STM32CubeProgrammer 期望的 External Loader 格式是 ELF,不是纯二进制!
# 参考 IAR EWARM 生成的 .stldr 文件,它们都是完整的 ELF 文件
#
# 解决方案:
#   直接复制 .elf 文件并重命名为 .stldr
#   ELF 格式已经包含了所有必要的信息:
#   - 段信息 (.dev_info, .text, .rodata, .data)
#   - 加载地址 (VMA/LMA)
#   - 符号表
#

param(
    [Parameter(Mandatory=$true)]
    [string]$ElfFile,
    
    [Parameter(Mandatory=$true)]
    [string]$OutputFile
)

Write-Host "Creating External Loader .stldr/.flm file (ELF format)..." -ForegroundColor Cyan
Write-Host "  Input: $ElfFile"
Write-Host "  Output: $OutputFile"

try {
    # 检查输入文件是否存在
    if (-not (Test-Path $ElfFile)) {
        throw "Input ELF file not found: $ElfFile"
    }
    
    # 方案 1: 如果输出是 .stldr,直接复制 ELF 文件
    if ($OutputFile -match '\.stldr$') {
        Write-Host "  [1/1] Copying ELF file to .stldr format..." -ForegroundColor Yellow
        Copy-Item $ElfFile $OutputFile -Force
        
        $fileInfo = Get-Item $OutputFile
        $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
        
        Write-Host ""
        Write-Host "  ✓ Generated: $OutputFile" -ForegroundColor Green
        Write-Host "  ✓ Size: $sizeKB KB" -ForegroundColor Green
        Write-Host "  ✓ Format: ELF (compatible with STM32CubeProgrammer)" -ForegroundColor Green
        
        exit 0
    }
    
    # 方案 2: 如果输出是 .flm,生成纯二进制格式 (兼容旧版工具)
    Write-Host "  Note: Generating legacy .flm binary format" -ForegroundColor Yellow
    Write-Host "  For STM32CubeProgrammer, .stldr (ELF) format is recommended" -ForegroundColor Yellow
    Write-Host ""
    
    # ARM GCC 工具路径
    $TOOLCHAIN_PATH = "D:\Devtools\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
    $OBJCOPY = Join-Path $TOOLCHAIN_PATH "arm-none-eabi-objcopy.exe"
    
    # 临时文件
    $DEV_INFO_BIN = $OutputFile -replace '\.flm$', '_dev_info.bin'
    $CODE_BIN = $OutputFile -replace '\.flm$', '_code.bin'
    
    # 1. 提取 .dev_info 段
    Write-Host "  [1/4] Extracting .dev_info section..." -ForegroundColor Yellow
    & $OBJCOPY -j .dev_info -O binary $ElfFile $DEV_INFO_BIN
    if ($LASTEXITCODE -ne 0) { throw "Failed to extract .dev_info" }
    
    # 2. 提取代码和数据段
    Write-Host "  [2/4] Extracting code sections..." -ForegroundColor Yellow
    & $OBJCOPY `
        -j .text -j .rodata -j .data -j .ARM -j .init_array -j .fini_array `
        -O binary `
        --change-addresses=-0x24000000 `
        $ElfFile $CODE_BIN
    if ($LASTEXITCODE -ne 0) { throw "Failed to extract code sections" }
    
    # 3. 合并文件
    Write-Host "  [3/4] Merging sections..." -ForegroundColor Yellow
    $devInfoBytes = [System.IO.File]::ReadAllBytes($DEV_INFO_BIN)
    $codeBytes = [System.IO.File]::ReadAllBytes($CODE_BIN)
    
    $outputStream = [System.IO.File]::Create($OutputFile)
    $outputStream.Write($devInfoBytes, 0, $devInfoBytes.Length)
    $outputStream.Write($codeBytes, 0, $codeBytes.Length)
    $outputStream.Close()
    
    # 4. 清理
    Write-Host "  [4/4] Cleaning up..." -ForegroundColor Yellow
    Remove-Item $DEV_INFO_BIN -ErrorAction SilentlyContinue
    Remove-Item $CODE_BIN -ErrorAction SilentlyContinue
    
    $fileInfo = Get-Item $OutputFile
    $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
    
    Write-Host ""
    Write-Host "  ✓ Generated: $OutputFile" -ForegroundColor Green
    Write-Host "  ✓ Size: $sizeKB KB" -ForegroundColor Green
    Write-Host "  ✓ Format: Binary (legacy format)" -ForegroundColor Yellow
    
    if ($sizeKB -lt 100) {
        Write-Host "  ✓ File size looks correct" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ Warning: File size seems large (expected < 100 KB)" -ForegroundColor Yellow
    }
    
    exit 0
    
} catch {
    Write-Host ""
    Write-Host "  ✗ Error: $_" -ForegroundColor Red
    [Environment]::Exit(1)
}

# 显式退出
[Environment]::Exit(0)
