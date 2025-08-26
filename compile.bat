@echo off
REM STM32 Project Build Script (enhanced)
REM Usage: compile.bat [build|clean|rebuild]

setlocal enabledelayedexpansion

if "%1"=="" (
    echo Usage: compile.bat [build^|clean^|rebuild]
    exit /b 1
)

set ACTION=%1

REM Project paths
set PROJECT_ROOT=%~dp0
REM Fix trailing backslash to avoid escaping closing quote when quoted
set IMPORT_PATH=%PROJECT_ROOT%
if "%IMPORT_PATH:~-1%"=="\" set IMPORT_PATH=%IMPORT_PATH:~0,-1%
REM Using trimmed IMPORT_PATH without trailing backslash
set PROJECT_NAME=swcode
set DEBUG_DIR=%PROJECT_ROOT%Debug
set MAKEFILE=%DEBUG_DIR%\makefile
set ELF_FILE=%DEBUG_DIR%\%PROJECT_NAME%.elf
set WORKSPACE=%PROJECT_ROOT%..\swcode-ide-ws

REM Toolchain/make from STM32CubeIDE (adjust if needed)
set ARM_TOOLCHAIN_PATH=D:\code\stm32\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin
set MAKE_PATH=D:\code\stm32\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin
set PATH=%ARM_TOOLCHAIN_PATH%;%MAKE_PATH%;%PATH%

REM STM32CubeIDE headless builder (stm32cubeidec.exe)
set STM32CUBEIDE_DIR=D:\code\stm32\STM32CubeIDE_1.19.0\STM32CubeIDE
if not exist "%STM32CUBEIDE_DIR%\stm32cubeidec.exe" (
    if exist "C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\stm32cubeidec.exe" (
        set STM32CUBEIDE_DIR=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE
    )
)
set IDEC=
if exist "%STM32CUBEIDE_DIR%\stm32cubeidec.exe" (
    set "IDEC=%STM32CUBEIDE_DIR%\stm32cubeidec.exe"
) else (
    for %%I in (stm32cubeidec.exe) do set "IDEC=%%~$PATH:I"
)

if not exist "%DEBUG_DIR%" mkdir "%DEBUG_DIR%"

if /i "%ACTION%"=="clean" goto do_clean
if /i "%ACTION%"=="build" goto do_build
if /i "%ACTION%"=="rebuild" goto do_rebuild

echo Error: Unknown argument "%ACTION%"
exit /b 1

:do_clean
if exist "%MAKEFILE%" (
    echo Cleaning project via make...
    pushd "%DEBUG_DIR%"
    make clean
    set ERR=%ERRORLEVEL%
    popd
    if "%ERR%" neq "0" (
        echo Error: Clean failed
        exit /b %ERR%
    )
    echo Clean completed successfully.
) else (
    echo No makefile found. Cleaning via STM32CubeIDE headless...
    call :headless_clean
    if errorlevel 1 exit /b 1
)
exit /b 0

:do_build
if exist "%MAKEFILE%" (
    echo Building project via make...
    pushd "%DEBUG_DIR%"
    make all -j%NUMBER_OF_PROCESSORS%
    set ERR=%ERRORLEVEL%
    popd
    if "%ERR%" neq "0" (
        echo Error: Build failed
        exit /b %ERR%
    )
) else (
    echo No makefile found. Generating and building via STM32CubeIDE headless...
    call :headless_build
    if errorlevel 1 exit /b 1
)
echo.
if exist "%ELF_FILE%" (
    echo Build completed successfully!
    echo Binary location: %ELF_FILE%
) else (
    echo Warning: Build finished but ELF not found: %ELF_FILE%
)
exit /b 0

:do_rebuild
echo Rebuilding project...
if exist "%MAKEFILE%" (
    pushd "%DEBUG_DIR%"
    make clean
    if errorlevel 1 (
        echo Error: Clean failed
        popd
        exit /b 1
    )
    make all -j%NUMBER_OF_PROCESSORS%
    set ERR=%ERRORLEVEL%
    popd
    if "%ERR%" neq "0" (
        echo Error: Build failed
        exit /b %ERR%
    )
) else (
    call :headless_clean
    if errorlevel 1 exit /b 1
    call :headless_build
    if errorlevel 1 exit /b 1
)
echo.
if exist "%ELF_FILE%" (
    echo Rebuild completed successfully!
    echo Binary location: %ELF_FILE%
) else (
    echo Warning: Rebuild finished but ELF not found: %ELF_FILE%
)
exit /b 0

:headless_build
if "%IDEC%"=="" (
    echo Error: stm32cubeidec.exe not found. Please adjust STM32CUBEIDE_DIR or add it to PATH.
    exit /b 1
)
echo IDEC="%IDEC%"
echo WORKSPACE="%WORKSPACE%"
echo PROJECT_ROOT="%PROJECT_ROOT%"
echo IMPORT_PATH="%IMPORT_PATH%"
echo Building via headless...
"%IDEC%" -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data "%WORKSPACE%" -import "%IMPORT_PATH%" -build "%PROJECT_NAME%/Debug"
exit /b %ERRORLEVEL%

:headless_clean
if "%IDEC%"=="" (
    echo Error: stm32cubeidec.exe not found. Please adjust STM32CUBEIDE_DIR or add it to PATH.
    exit /b 1
)
echo Cleaning via headless...
"%IDEC%" -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data "%WORKSPACE%" -import "%IMPORT_PATH%" -cleanBuild "%PROJECT_NAME%/Debug"
exit /b %ERRORLEVEL%

endlocal