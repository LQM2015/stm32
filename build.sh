#!/bin/bash

# Toolchain Path (IDE bundled GCC 13.3.1)
TOOLCHAIN_PATH="/opt/st/stm32cubeide_2.0.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.100.202509120712/tools/bin"

# Export PATH so make uses the correct compiler
export PATH="$TOOLCHAIN_PATH:$PATH"

# Ensure we are compiling in the Debug directory
if [ -d "Debug" ]; then
    cd Debug
elif [ -d "../Debug" ]; then
    # In case script is run from inside Debug/ or similar, though assuming root is safer
    cd ../Debug
else
    echo "Error: 'Debug' folder not found. Please run this script from the project root."
    exit 1
fi

# Print GCC version for verification
echo "Using Compiler:"
arm-none-eabi-gcc --version | head -n 1
echo "------------------------------------------------"

# Handle arguments
if [ "$1" == "clean" ]; then
    echo "Cleaning build artifacts..."
    make clean
elif [ "$1" == "rebuild" ]; then
    echo "Rebuilding project..."
    make clean
    make -j$(nproc) all
else
    echo "Compiling project..."
    # Use all available cores
    make -j$(nproc) all
fi
