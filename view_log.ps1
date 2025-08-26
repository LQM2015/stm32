# Wio_Lite Log Viewer PowerShell Script
# Usage: .\view_log.ps1 [options]

param(
    [switch]$Watch,      # Watch mode - monitor log changes
    [int]$Lines = 50,    # Number of lines to display
    [switch]$All         # Show all content
)

$LogPath = "D:\Logs"

function Get-LatestLogFile {
    $logFiles = Get-ChildItem -Path $LogPath -Name "Wio_Lite_*.log" -ErrorAction SilentlyContinue
    if ($logFiles) {
        $latest = $logFiles | ForEach-Object { 
            Get-Item (Join-Path $LogPath $_) 
        } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        return $latest.FullName
    }
    return $null
}

function Show-LogContent {
    param($FilePath, $LineCount)
    
    Write-Host "===============================================" -ForegroundColor Cyan
    Write-Host "      Wio_Lite Log Viewer" -ForegroundColor Yellow
    Write-Host "===============================================" -ForegroundColor Cyan
    Write-Host "File: $(Split-Path $FilePath -Leaf)" -ForegroundColor Green
    Write-Host "Path: $FilePath" -ForegroundColor Gray
    Write-Host "Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
    Write-Host "===============================================" -ForegroundColor Cyan
    Write-Host ""
    
    if ($All) {
        Get-Content $FilePath
    } else {
        Get-Content $FilePath -Tail $LineCount
    }
    
    Write-Host ""
    Write-Host "===============================================" -ForegroundColor Cyan
    Write-Host "End of log content" -ForegroundColor Yellow
    Write-Host "===============================================" -ForegroundColor Cyan
}

function Watch-LogFile {
    param($FilePath)
    
    Write-Host "Watching log file for changes..." -ForegroundColor Yellow
    Write-Host "Press Ctrl+C to stop" -ForegroundColor Red
    Write-Host ""
    
    $lastPosition = 0
    if (Test-Path $FilePath) {
        $lastPosition = (Get-Item $FilePath).Length
    }
    
    while ($true) {
        Start-Sleep -Seconds 1
        
        if (Test-Path $FilePath) {
            $currentLength = (Get-Item $FilePath).Length
            if ($currentLength -gt $lastPosition) {
                # New content available
                $newContent = Get-Content $FilePath -Raw
                $newLines = $newContent.Substring($lastPosition)
                Write-Host $newLines -NoNewline -ForegroundColor White
                $lastPosition = $currentLength
            }
        }
        
        # Check if a newer log file exists
        $currentLatest = Get-LatestLogFile
        if ($currentLatest -ne $FilePath) {
            Write-Host "`n`nNew log file detected: $(Split-Path $currentLatest -Leaf)" -ForegroundColor Green
            $FilePath = $currentLatest
            $lastPosition = 0
            Show-LogContent -FilePath $FilePath -LineCount 10
        }
    }
}

# Main execution
$latestLog = Get-LatestLogFile

if (-not $latestLog) {
    Write-Host "No Wio_Lite log files found in $LogPath" -ForegroundColor Red
    exit 1
}

if ($Watch) {
    Show-LogContent -FilePath $latestLog -LineCount $Lines
    Watch-LogFile -FilePath $latestLog
} else {
    Show-LogContent -FilePath $latestLog -LineCount $Lines
}
