################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Startup/startup_stm32h725aeix.s 

OBJS += \
./Core/Startup/startup_stm32h725aeix.o 

S_DEPS += \
./Core/Startup/startup_stm32h725aeix.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Startup/%.o: ../Core/Startup/%.s Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m7 -g3 -DDEBUG -c -I../Core/Inc -I../FATFS/Target -I../FATFS/App -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/include -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FatFs/src -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Device/ST/STM32H7xx/Include -ID:/code/stm32/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Include -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/include -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/Third_Party/FatFs/src -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Device/ST/STM32H7xx/Include -IC:/Users/lingq/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Include -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Third_Party/FatFs/src -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Core-2f-Startup

clean-Core-2f-Startup:
	-$(RM) ./Core/Startup/startup_stm32h725aeix.d ./Core/Startup/startup_stm32h725aeix.o

.PHONY: clean-Core-2f-Startup

