@echo off
REM STM32 Project Build Script
REM Usage: compile.bat [build|clean|rebuild]

setlocal enabledelayedexpansion

REM Check if argument is provided
if "%1"=="" (
    echo Usage: compile.bat [build^|clean^|rebuild]
    echo   build   - Build the project
    echo   clean   - Clean build artifacts
    echo   rebuild - Clean and build
    exit /b 1
)

REM Set toolchain path - adjust this path according to your STM32CubeIDE installation
set ARM_TOOLCHAIN_PATH=D:\code\stm32\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin
set MAKE_PATH=D:\code\stm32\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin
set PATH=%ARM_TOOLCHAIN_PATH%;%MAKE_PATH%;%PATH%

REM Check if arm-none-eabi-gcc is available
where arm-none-eabi-gcc >nul 2>&1
if errorlevel 1 (
    echo Error: ARM toolchain not found. Please check the ARM_TOOLCHAIN_PATH in this script.
    echo Current path: %ARM_TOOLCHAIN_PATH%
    echo.
    echo Alternative paths to try:
    echo   - C:\ST\STM32CubeIDE_*\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*\tools\bin
    echo   - Add STM32CubeIDE toolchain to your system PATH
    exit /b 1
)

REM Change to Debug directory where makefile is located
cd /d "%~dp0Debug"
if errorlevel 1 (
    echo Error: Cannot change to Debug directory
    exit /b 1
)

REM Execute based on argument
if /i "%1"=="clean" (
    echo Cleaning project...
    make clean
    if errorlevel 1 (
        echo Error: Clean failed
        exit /b 1
    )
    echo Clean completed successfully.
    
) else if /i "%1"=="build" (
    echo Building project...
    make all -j%NUMBER_OF_PROCESSORS%
    if errorlevel 1 (
        echo Error: Build failed
        exit /b 1
    )
    echo.
    echo Build completed successfully!
    echo Binary location: %~dp0Debug\swcode.elf
    
) else if /i "%1"=="rebuild" (
    echo Rebuilding project...
    make clean
    if errorlevel 1 (
        echo Error: Clean failed
        exit /b 1
    )
    echo Clean completed, now building...
    make all -j%NUMBER_OF_PROCESSORS%
    if errorlevel 1 (
        echo Error: Build failed
        exit /b 1
    )
    echo.
    echo Rebuild completed successfully!
    echo Binary location: %~dp0Debug\swcode.elf
    
) else (
    echo Error: Unknown argument "%1"
    echo Usage: compile.bat [build^|clean^|rebuild]
    exit /b 1
)

endlocal
