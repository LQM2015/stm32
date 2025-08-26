@echo off
echo ===============================================
echo      Latest Wio_Lite Log Viewer
echo ===============================================
echo.

REM Find the latest Wio_Lite log file
for /f "delims=" %%i in ('dir "D:\Logs\Wio_Lite_*.log" /b /o-d 2^>nul ^| findstr /n "^" ^| findstr "^1:"') do (
    set "latest_log=%%i"
    set "latest_log=!latest_log:~2!"
)

if defined latest_log (
    echo Latest log file: %latest_log%
    echo File location: D:\Logs\%latest_log%
    echo.
    echo Press 'q' to quit, 'r' to refresh
    echo ===============================================
    echo.
    
    REM Display last 50 lines of the latest log
    powershell -Command "Get-Content 'D:\Logs\%latest_log%' -Tail 50"
    
    echo.
    echo ===============================================
    echo End of log file: %latest_log%
    echo ===============================================
) else (
    echo No Wio_Lite log files found in D:\Logs\
)

pause
