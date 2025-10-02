# Verify STM32 External Loader File
# This script verifies that the .stldr file has the correct format

param(
    [string]$StldrPath = "FlashLoader_Debug\ext_burn.stldr"
)

$ErrorActionPreference = 'Stop'

Write-Host "=== STM32 External Loader Verification Script ===" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $StldrPath)) {
    Write-Host "ERROR: File not found: $StldrPath" -ForegroundColor Red
    exit 1
}

$file = Get-Item $StldrPath
Write-Host "File: $($file.FullName)" -ForegroundColor Green
Write-Host "Size: $($file.Length) bytes"
Write-Host "Modified: $($file.LastWriteTime)"
Write-Host ""

# Read file
$bytes = [System.IO.File]::ReadAllBytes($file.FullName)

# Check if it's ELF format (CRITICAL!)
Write-Host "=== File Format Check ===" -ForegroundColor Cyan
Write-Host ""
$isELF = ($bytes[0] -eq 0x7F -and $bytes[1] -eq 0x45 -and $bytes[2] -eq 0x4C -and $bytes[3] -eq 0x46)
Write-Host "Magic Number: 0x$($bytes[0].ToString('X2')) 0x$($bytes[1].ToString('X2')) 0x$($bytes[2].ToString('X2')) 0x$($bytes[3].ToString('X2'))"
Write-Host "ASCII: $([System.Text.Encoding]::ASCII.GetString($bytes[0..3]))"
if ($isELF) {
    Write-Host "Format: ✓ ELF file (CORRECT!)" -ForegroundColor Green
    Write-Host "  EI_CLASS: $($bytes[4]) $(if ($bytes[4] -eq 1) {'(32-bit)'} elseif ($bytes[4] -eq 2) {'(64-bit)'} else {'(unknown)'})"
    Write-Host "  EI_DATA: $($bytes[5]) $(if ($bytes[5] -eq 1) {'(Little Endian)'} elseif ($bytes[5] -eq 2) {'(Big Endian)'} else {'(unknown)'})"
} else {
    Write-Host "Format: ✗ NOT ELF format (PROBLEM!)" -ForegroundColor Red
    Write-Host "  STM32CubeProgrammer requires ELF format .stldr files!" -ForegroundColor Red
    Write-Host "  Your file appears to be raw binary format." -ForegroundColor Yellow
}

Write-Host ""

# Parse StorageInfo structure
Write-Host "=== StorageInfo Structure ===" -ForegroundColor Cyan
Write-Host ""

# For ELF files, we need to find the .dev_info section
# For now, we'll look for the DeviceName pattern
$devInfoOffset = 0
if ($isELF) {
    Write-Host "Searching for .dev_info section in ELF file..." -ForegroundColor Yellow
    # Look for the DeviceName pattern (starts with printable characters)
    for ($i = 0; $i -lt ($bytes.Length - 100); $i++) {
        $possibleName = [System.Text.Encoding]::ASCII.GetString($bytes[$i..($i+99)])
        if ($possibleName -match '^[A-Za-z0-9_\-]+') {
            # Found potential DeviceName
            $devInfoOffset = $i
            Write-Host "  Found potential .dev_info at offset 0x$($devInfoOffset.ToString('X8'))" -ForegroundColor Green
            break
        }
    }
}

Write-Host ""

# DeviceName[100]
$deviceName = [System.Text.Encoding]::ASCII.GetString($bytes[$devInfoOffset..($devInfoOffset+99)]).TrimEnd([char]0)
Write-Host "DeviceName: '$deviceName'" -ForegroundColor $(if ($deviceName) {'Green'} else {'Red'})

# DeviceType (uint16 at offset 100)
$devType = [BitConverter]::ToUInt16($bytes, $devInfoOffset + 100)
$devTypeNames = @{
    0x00 = "UNKNOWN"
    0x01 = "RAM"
    0x02 = "FLASH"
    0x06 = "NOR_FLASH"
    0x07 = "NAND_FLASH"
    0x0B = "SPI_FLASH"
}
$devTypeName = if ($devTypeNames.ContainsKey($devType)) { $devTypeNames[$devType] } else { "UNKNOWN_TYPE" }
Write-Host "DeviceType: 0x$($devType.ToString('X4')) ($devTypeName)" -ForegroundColor $(if ($devType -eq 0x0B) {'Green'} else {'Yellow'})

# DeviceStartAddress (uint32 at offset 102)
$startAddr = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 102)
Write-Host "DeviceStartAddress: 0x$($startAddr.ToString('X8'))" -ForegroundColor $(if ($startAddr -eq 0x90000000) {'Green'} else {'Red'})

# DeviceSize (uint32 at offset 106)
$devSize = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 106)
$devSizeMB = [math]::Round($devSize / 1MB, 2)
Write-Host "DeviceSize: 0x$($devSize.ToString('X8')) ($devSizeMB MB)" -ForegroundColor Green

# PageSize (uint32 at offset 110)
$pageSize = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 110)
Write-Host "PageSize: 0x$($pageSize.ToString('X8')) ($pageSize bytes)" -ForegroundColor $(if ($pageSize -gt 0) {'Green'} else {'Red'})

# EraseValue (uint8 at offset 114)
$eraseValue = $bytes[$devInfoOffset + 114]
Write-Host "EraseValue: 0x$($eraseValue.ToString('X2'))" -ForegroundColor $(if ($eraseValue -eq 0xFF) {'Green'} else {'Yellow'})

# PageProgramTime (uint32 at offset 115)
$progTime = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 115)
Write-Host "PageProgramTime: $progTime (units of 100us)"

# SectorEraseTime (uint32 at offset 119)
$eraseTime = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 119)
Write-Host "SectorEraseTime: $eraseTime ms"

# ChipEraseTime (uint32 at offset 123)
$chipEraseTime = [BitConverter]::ToUInt32($bytes, $devInfoOffset + 123)
Write-Host "ChipEraseTime: $chipEraseTime ms"

# SectorInfo array (starts at offset 127)
Write-Host ""
Write-Host "=== SectorInfo Array ===" -ForegroundColor Cyan
$offset = $devInfoOffset + 127
$sectorNum = 0
$allZero = $true
while ($sectorNum -lt 10 -and $offset + 7 -lt $bytes.Length) {
    $sectorSize = [BitConverter]::ToUInt32($bytes, $offset)
    $sectorCount = [BitConverter]::ToUInt32($bytes, $offset + 4)
    
    if ($sectorSize -ne 0 -or $sectorCount -ne 0) {
        $allZero = $false
        $totalSize = [uint64]$sectorSize * [uint64]$sectorCount
        $totalSizeMB = [math]::Round($totalSize / 1MB, 2)
        Write-Host "  Sector[$sectorNum]: Size=0x$($sectorSize.ToString('X8')) ($([math]::Round($sectorSize/1KB, 1))KB), Count=$sectorCount, Total=$totalSizeMB MB" -ForegroundColor Green
    }
    
    $offset += 8
    $sectorNum++
}

if ($allZero) {
    Write-Host "  ERROR: No sector information defined!" -ForegroundColor Red
}

# Validation summary
Write-Host ""
Write-Host "=== Validation Summary ===" -ForegroundColor Cyan
$issues = @()

if (-not $isELF) {
    $issues += "File is NOT in ELF format (CRITICAL! STM32CubeProgrammer needs ELF)"
}

if ([string]::IsNullOrWhiteSpace($deviceName)) {
    $issues += "DeviceName is empty"
}

if ($devType -ne 0x0B) {
    $issues += "DeviceType should be 0x0B (SPI_FLASH) for W25Q256"
}

if ($startAddr -ne 0x90000000) {
    $issues += "DeviceStartAddress should be 0x90000000 for STM32H750 QSPI"
}

if ($pageSize -eq 0) {
    $issues += "PageSize cannot be zero"
}

if ($allZero) {
    $issues += "No sector information defined"
}

if ($issues.Count -eq 0) {
    Write-Host "✓ All checks passed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "The loader file appears to be correctly formatted." -ForegroundColor Green
    Write-Host ""
    Write-Host "If STM32CubeProgrammer still doesn't recognize it:" -ForegroundColor Yellow
    Write-Host "  1. Make sure the file is in: [STM32CubeProgrammer]\bin\ExternalLoader\"
    Write-Host "  2. Restart STM32CubeProgrammer"
    Write-Host "  3. Check STM32CubeProgrammer version (needs 2.7.0 or later)"
    Write-Host "  4. Try with different file name: W25Q256_STM32H750_0x90000000.stldr"
    Write-Host "  5. Check if your MCU is correctly detected (should show STM32H750)"
} else {
    Write-Host "✗ Issues found:" -ForegroundColor Red
    foreach ($issue in $issues) {
        Write-Host "  - $issue" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "=== Hex Dump (first 160 bytes) ===" -ForegroundColor Cyan
for ($i = 0; $i -lt [math]::Min(160, $bytes.Length); $i += 16) {
    $hex = ($bytes[$i..([math]::Min($i+15, $bytes.Length-1))] | ForEach-Object { $_.ToString("X2") }) -join " "
    $ascii = -join ($bytes[$i..([math]::Min($i+15, $bytes.Length-1))] | ForEach-Object { 
        if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { '.' }
    })
    Write-Host ("{0:X4}: {1,-47} {2}" -f $i, $hex, $ascii)
}
