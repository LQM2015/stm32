@echo off
REM ========================================
REM STM32H750 Bootloader Build and Flash Script
REM ========================================

setlocal enabledelayedexpansion

REM Configuration variables
set PROJECT_NAME=H750XBH6
set BOOTLOADER_DIR=Debug_Bootloader
set EXTFLASH_DIR=Debug_ExtFlash
set PROGRAMMER=STM32_Programmer_CLI

echo ========================================
echo STM32H750 Bootloader Build Script
echo ========================================
echo.

:MENU
echo Please select operation:
echo 1. Build Bootloader
echo 2. Build Application (External Flash)
echo 3. Build All (Bootloader + Application)
echo 4. Flash Bootloader
echo 5. Flash Application
echo 6. Flash All
echo 7. Clean Build Output
echo 8. Exit
echo.

set /p choice="Enter option (1-8): "

if "%choice%"=="1" goto BUILD_BOOTLOADER
if "%choice%"=="2" goto BUILD_APPLICATION
if "%choice%"=="3" goto BUILD_ALL
if "%choice%"=="4" goto FLASH_BOOTLOADER
if "%choice%"=="5" goto FLASH_APPLICATION
if "%choice%"=="6" goto FLASH_ALL
if "%choice%"=="7" goto CLEAN
if "%choice%"=="8" goto END

echo Invalid option, please try again
goto MENU

:BUILD_BOOTLOADER
echo.
echo [1/1] Building Bootloader...
echo ----------------------------------------
REM Adjust commands based on your build tool
REM Example: using make or STM32CubeIDE CLI
echo Please build Bootloader configuration in STM32CubeIDE manually
echo Or uncomment the command below after configuring makefile:
REM make -C %BOOTLOADER_DIR% clean all
echo ----------------------------------------
pause
goto MENU

:BUILD_APPLICATION
echo.
echo [1/1] Building Application...
echo ----------------------------------------
echo Please build ExtFlash configuration in STM32CubeIDE manually
echo Or uncomment the command below after configuring makefile:
REM make -C %EXTFLASH_DIR% clean all
echo ----------------------------------------
pause
goto MENU

:BUILD_ALL
echo.
echo [1/2] Building Bootloader...
echo ----------------------------------------
echo Please build Bootloader configuration in STM32CubeIDE first
pause
echo.
echo [2/2] Building Application...
echo ----------------------------------------
echo Then build ExtFlash configuration
pause
goto MENU

:FLASH_BOOTLOADER
echo.
echo [1/1] Flashing Bootloader to Internal Flash...
echo ----------------------------------------
if not exist "%BOOTLOADER_DIR%\%PROJECT_NAME%.hex" (
    echo ERROR: Bootloader hex file not found
    echo Please build Bootloader first
    pause
    goto MENU
)

echo Connecting to debugger...
%PROGRAMMER% -c port=SWD -w %BOOTLOADER_DIR%\%PROJECT_NAME%.hex -v -s

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Bootloader flashed successfully!
) else (
    echo.
    echo [FAIL] Bootloader flash failed!
    echo Error code: %ERRORLEVEL%
)
echo ----------------------------------------
pause
goto MENU

:FLASH_APPLICATION
echo.
echo [1/1] Flashing Application to External Flash...
echo ----------------------------------------
if not exist "%EXTFLASH_DIR%\%PROJECT_NAME%.hex" (
    echo ERROR: Application hex file not found
    echo Please build Application first
    pause
    goto MENU
)

echo WARNING: External Flash programming requires External Loader
echo Please ensure W25Q256 External Loader is installed
echo.
set /p confirm="Continue? (Y/N): "
if /i not "%confirm%"=="Y" goto MENU

echo Connecting to debugger and loading External Loader...
REM Note: Specify correct External Loader path
REM %PROGRAMMER% -c port=SWD -el "path\to\W25Q256.stldr"
%PROGRAMMER% -c port=SWD -w %EXTFLASH_DIR%\%PROJECT_NAME%.hex 0x90000000 -v

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Application flashed successfully!
) else (
    echo.
    echo [FAIL] Application flash failed!
    echo Error code: %ERRORLEVEL%
    echo.
    echo Possible reasons:
    echo 1. External Loader not configured
    echo 2. QSPI Flash hardware connection issue
    echo 3. Bootloader not flashed
)
echo ----------------------------------------
pause
goto MENU

:FLASH_ALL
echo.
echo [1/2] Flashing Bootloader...
echo ----------------------------------------
call :FLASH_BOOTLOADER
echo.
echo [2/2] Flashing Application...
echo ----------------------------------------
call :FLASH_APPLICATION
goto MENU

:CLEAN
echo.
echo Cleaning build output...
echo ----------------------------------------
if exist "%BOOTLOADER_DIR%" (
    echo Cleaning Bootloader directory...
    rmdir /s /q "%BOOTLOADER_DIR%"
)
if exist "%EXTFLASH_DIR%" (
    echo Cleaning Application directory...
    rmdir /s /q "%EXTFLASH_DIR%"
)
echo [OK] Clean completed
echo ----------------------------------------
pause
goto MENU

:END
echo.
echo Thank you for using! Goodbye!
endlocal
exit /b 0
