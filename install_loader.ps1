# Install STM32 External Loader
# This script copies the .stldr file to STM32CubeProgrammer's ExternalLoader directory

param(
    [string]$CubeProgrammerPath = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer",
    [string]$LoaderName = "W25Q256_STM32H750_0x90000000.stldr"
)

$ErrorActionPreference = 'Stop'

Write-Host "=== STM32 External Loader Installation Script ===" -ForegroundColor Cyan
Write-Host ""

# Check if source file exists
$sourcePath = Join-Path $PSScriptRoot "FlashLoader_Debug\ext_burn.stldr"
if (-not (Test-Path $sourcePath)) {
    Write-Host "ERROR: Source file not found: $sourcePath" -ForegroundColor Red
    Write-Host "Please build the project first!" -ForegroundColor Yellow
    exit 1
}

$sourceFile = Get-Item $sourcePath
Write-Host "Source file: $($sourceFile.FullName)" -ForegroundColor Green
Write-Host "  Size: $($sourceFile.Length) bytes"
Write-Host "  Modified: $($sourceFile.LastWriteTime)"
Write-Host ""

# Find STM32CubeProgrammer installation
$loaderDir = Join-Path $CubeProgrammerPath "bin\ExternalLoader"

if (-not (Test-Path $loaderDir)) {
    Write-Host "ERROR: STM32CubeProgrammer ExternalLoader directory not found!" -ForegroundColor Red
    Write-Host "Expected: $loaderDir" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please check:" -ForegroundColor Yellow
    Write-Host "  1. STM32CubeProgrammer is installed"
    Write-Host "  2. Installation path is correct"
    Write-Host "  3. Use -CubeProgrammerPath parameter to specify custom path"
    Write-Host ""
    Write-Host "Common paths:" -ForegroundColor Cyan
    Write-Host "  Windows: C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer"
    Write-Host "  Linux: /opt/stm32cubeprg"
    Write-Host "  macOS: /Applications/STMicroelectronics/STM32CubeProgrammer.app/Contents/Java"
    exit 1
}

Write-Host "Target directory: $loaderDir" -ForegroundColor Green
Write-Host ""

# Copy file
$destPath = Join-Path $loaderDir $LoaderName
Write-Host "Installing loader as: $LoaderName" -ForegroundColor Cyan

try {
    Copy-Item $sourcePath $destPath -Force
    Write-Host "SUCCESS: Loader installed!" -ForegroundColor Green
    
    $installedFile = Get-Item $destPath
    Write-Host ""
    Write-Host "Installed file: $($installedFile.FullName)" -ForegroundColor Green
    Write-Host "  Size: $($installedFile.Length) bytes"
    Write-Host ""
    
    Write-Host "=== Next Steps ===" -ForegroundColor Cyan
    Write-Host "1. Restart STM32CubeProgrammer if it's running"
    Write-Host "2. Connect your STM32H750 board"
    Write-Host "3. Click the 'External loaders' button (icon in left panel)"
    Write-Host "4. Look for '$LoaderName' in the list"
    Write-Host "5. Check the checkbox to enable it"
    Write-Host "6. Start programming at address 0x90000000"
    Write-Host ""
    
    # List all loaders in the directory
    Write-Host "=== Available External Loaders ===" -ForegroundColor Cyan
    Get-ChildItem $loaderDir -Filter "*.stldr" | 
        Select-Object Name, @{Name='Size (KB)';Expression={[math]::Round($_.Length/1KB, 2)}}, LastWriteTime |
        Format-Table -AutoSize
        
} catch {
    Write-Host "ERROR: Failed to copy file!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host "Installation complete!" -ForegroundColor Green
