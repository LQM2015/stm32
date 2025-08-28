################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Shell/src/shell.c \
../Shell/src/shell_cmd_list.c \
../Shell/src/shell_commands.c \
../Shell/src/shell_ext.c \
../Shell/src/shell_log.c \
../Shell/src/shell_port.c 

OBJS += \
./Shell/src/shell.o \
./Shell/src/shell_cmd_list.o \
./Shell/src/shell_commands.o \
./Shell/src/shell_ext.o \
./Shell/src/shell_log.o \
./Shell/src/shell_port.o 

C_DEPS += \
./Shell/src/shell.d \
./Shell/src/shell_cmd_list.d \
./Shell/src/shell_commands.d \
./Shell/src/shell_ext.d \
./Shell/src/shell_log.d \
./Shell/src/shell_port.d 


# Each subdirectory must supply rules for building sources it contributes
Shell/src/%.o Shell/src/%.su Shell/src/%.cyclo: ../Shell/src/%.c Shell/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_HAL_DRIVER -DSTM32H725xx -DSTM32_THREAD_SAFE_STRATEGY=4 '-DSHELL_CFG_USER="shell_cfg_user.h"' -c -I../Core/Inc -I../FATFS/Target -I../FATFS/App -I../Core/ThreadSafe -I../Shell/inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Third_Party/FatFs/src -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Shell-2f-src

clean-Shell-2f-src:
	-$(RM) ./Shell/src/shell.cyclo ./Shell/src/shell.d ./Shell/src/shell.o ./Shell/src/shell.su ./Shell/src/shell_cmd_list.cyclo ./Shell/src/shell_cmd_list.d ./Shell/src/shell_cmd_list.o ./Shell/src/shell_cmd_list.su ./Shell/src/shell_commands.cyclo ./Shell/src/shell_commands.d ./Shell/src/shell_commands.o ./Shell/src/shell_commands.su ./Shell/src/shell_ext.cyclo ./Shell/src/shell_ext.d ./Shell/src/shell_ext.o ./Shell/src/shell_ext.su ./Shell/src/shell_log.cyclo ./Shell/src/shell_log.d ./Shell/src/shell_log.o ./Shell/src/shell_log.su ./Shell/src/shell_port.cyclo ./Shell/src/shell_port.d ./Shell/src/shell_port.o ./Shell/src/shell_port.su

.PHONY: clean-Shell-2f-src

