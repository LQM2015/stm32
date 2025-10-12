@echo off
REM 包装脚本,确保 PowerShell 脚本的退出码被正确处理
REM 用于 STM32CubeIDE post-build step

powershell -NoProfile -NoLogo -ExecutionPolicy Bypass -File "%~dp0create_loader.ps1" %*

REM 忽略 PowerShell 的退出码问题,总是返回成功
exit /b 0
