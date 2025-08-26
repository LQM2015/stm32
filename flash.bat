@echo off
REM STM32 Flash Programming Script - Simplified Version
REM Usage: flash.bat [flash|erase|verify|info|list]

setlocal enabledelayedexpansion

REM Set STM32CubeProgrammer path
set STM32_PROGRAMMER="D:\code\stm32\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

REM Set project paths
set PROJECT_ROOT=%~dp0
set ELF_FILE=%PROJECT_ROOT%Debug\swcode.elf

REM Check if STM32CubeProgrammer CLI exists
if not exist %STM32_PROGRAMMER% (
    echo Error: STM32_Programmer_CLI.exe not found at:
    echo %STM32_PROGRAMMER%
    exit /b 1
)

REM Check if argument is provided
if "%1"=="" (
    echo STM32 Flash Programming Tool
    echo.
    echo Usage: flash.bat [flash^|erase^|verify^|info^|list]
    echo   flash   - Program the MCU with compiled firmware
    echo   erase   - Erase the entire flash memory
    echo   verify  - Verify programmed flash against firmware
    echo   info    - Display MCU and connection information
    echo   list    - List all available ST-Link connections
    echo.
    echo Target: STM32H725AEIX
    exit /b 1
)

REM Execute based on argument
if /i "%1"=="list" (
    echo Scanning for available ST-Link connections...
    %STM32_PROGRAMMER% -l
    goto :end
    
) else if /i "%1"=="info" (
    echo Connecting to STM32 device...
    %STM32_PROGRAMMER% -c port=SWD
    goto :end
    
) else if /i "%1"=="erase" (
    echo WARNING: This will erase all flash content!
    choice /C YN /M "Continue with full erase"
    if errorlevel 2 (
        echo Operation cancelled.
        goto :end
    )
    
    echo Erasing STM32 flash memory...
    %STM32_PROGRAMMER% -c port=SWD -e all
    goto :end
    
) else if /i "%1"=="verify" (
    if not exist "%ELF_FILE%" (
        echo Error: ELF file not found: %ELF_FILE%
        echo Please build the project first using: compile.bat build
        goto :error
    )
    
    echo Verifying programmed firmware...
    %STM32_PROGRAMMER% -c port=SWD -v "%ELF_FILE%"
    goto :end
    
) else if /i "%1"=="flash" (
    echo Programming STM32H725 with latest firmware...
    
    REM Check if ELF file exists
    if not exist "%ELF_FILE%" (
        echo Error: ELF file not found: %ELF_FILE%
        echo Please build the project first using: compile.bat build
        echo.
        choice /C YN /M "Build project now"
        if errorlevel 2 (
            echo Programming cancelled.
            goto :error
        )
        
        echo Building project...
        call "%PROJECT_ROOT%compile.bat" build
        if errorlevel 1 (
            echo Error: Build failed
            goto :error
        )
    )
    
    echo.
    echo Target: STM32H725AEIX
    echo File: %ELF_FILE%
    echo Interface: ST-Link (SWD)
    echo.
    
    echo Programming and verifying...
    %STM32_PROGRAMMER% -c port=SWD -w "%ELF_FILE%" -v -rst
    
    if errorlevel 1 (
        echo Error: Programming failed
        goto :error
    )
    
    echo.
    echo ========================================
    echo PROGRAMMING COMPLETED SUCCESSFULLY!
    echo ========================================
    echo.
    echo Target: STM32H725AEIX  
    echo Firmware: %ELF_FILE%
    echo Status: Device programmed and reset
    echo.
    echo You can now test the USB functionality with the new firmware.
    echo Use 'usb_debug' command in the serial terminal to check USB status.
    goto :end
    
) else (
    echo Error: Unknown command "%1"
    echo Valid commands: flash, erase, verify, info, list
    goto :error
)

:end
echo.
echo Operation completed at %date% %time%
endlocal
exit /b 0

:error
echo.
echo Operation failed at %date% %time%
endlocal
exit /b 1
