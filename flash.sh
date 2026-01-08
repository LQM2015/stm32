#!/bin/bash

# STM32 Flash Programming Script for Linux
# Usage: ./flash.sh [flash|erase|verify|info|list]

# Set STM32CubeProgrammer path
STM32_PROGRAMMER="/usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"

# Set project paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ELF_FILE="${PROJECT_ROOT}/Debug/swcode.elf"

# Check if STM32CubeProgrammer CLI exists
if [ ! -f "$STM32_PROGRAMMER" ]; then
    echo "Error: STM32_Programmer_CLI not found at:"
    echo "$STM32_PROGRAMMER"
    echo "Please update the path in this script."
    exit 1
fi

# Check if argument is provided
if [ -z "$1" ]; then
    echo "STM32 Flash Programming Tool (Linux)"
    echo ""
    echo "Usage: ./flash.sh [flash|erase|verify|info|list]"
    echo "  flash   - Program the MCU with compiled firmware"
    echo "  erase   - Erase the entire flash memory"
    echo "  verify  - Verify programmed flash against firmware"
    echo "  info    - Display MCU and connection information"
    echo "  list    - List all available ST-Link connections"
    echo ""
    echo "Target: STM32H725AEIX"
    exit 1
fi

# Execute based on argument
case "$1" in
    list)
        echo "Scanning for available ST-Link connections..."
        "$STM32_PROGRAMMER" -l
        ;;
        
    info)
        echo "Connecting to STM32 device..."
        "$STM32_PROGRAMMER" -c port=SWD freq=1000 mode=UR
        ;;
        
    erase)
        echo "WARNING: This will erase all flash content!"
        read -p "Continue with full erase? (y/n) " -n 1 -r
        echo ""
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo "Erasing STM32 flash memory..."
            "$STM32_PROGRAMMER" -c port=SWD freq=1000 mode=UR -e all
        else
            echo "Operation cancelled."
        fi
        ;;
        
    verify)
        if [ ! -f "$ELF_FILE" ]; then
            echo "Error: ELF file not found: $ELF_FILE"
            echo "Please build the project first using: ./build.sh"
            exit 1
        fi
        
        echo "Verifying programmed firmware..."
        "$STM32_PROGRAMMER" -c port=SWD freq=1000 mode=UR -v "$ELF_FILE"
        ;;
        
    flash)
        echo "Programming STM32H725 with latest firmware..."
        
        # Check if ELF file exists
        if [ ! -f "$ELF_FILE" ]; then
            echo "Error: ELF file not found: $ELF_FILE"
            echo "Please build the project first using: ./build.sh"
            echo ""
            read -p "Build project now? (y/n) " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                echo "Building project..."
                "${PROJECT_ROOT}/build.sh"
                if [ $? -ne 0 ]; then
                    echo "Error: Build failed"
                    exit 1
                fi
            else
                echo "Programming cancelled."
                exit 1
            fi
        fi
        
        echo ""
        echo "Target: STM32H725AEIX"
        echo "File: $ELF_FILE"
        echo "Interface: ST-Link (SWD)"
        echo ""
        
        echo "Programming and verifying..."
        "$STM32_PROGRAMMER" -c port=SWD freq=1000 mode=UR -w "$ELF_FILE" -v -rst
        
        if [ $? -eq 0 ]; then
            echo ""
            echo "========================================"
            echo "PROGRAMMING COMPLETED SUCCESSFULLY!"
            echo "========================================"
            echo ""
            echo "Target: STM32H725AEIX"
            echo "Firmware: $(basename "$ELF_FILE")"
            echo "Status: Device programmed and reset"
        else
            echo "Error: Programming failed"
            exit 1
        fi
        ;;
        
    *)
        echo "Error: Unknown command \"$1\""
        echo "Valid commands: flash, erase, verify, info, list"
        exit 1
        ;;
esac
