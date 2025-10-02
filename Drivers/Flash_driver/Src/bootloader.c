/**
  ******************************************************************************
  * @file    bootloader.c
  * @brief   Bootloader implementation for STM32H750
  *          This file contains functions to jump from internal flash to 
  *          external QSPI flash application
  ******************************************************************************
  */

#include "bootloader.h"
#include "qspi_w25q256.h"
#include <stdio.h>

/* 函数指针类型定义 */
typedef void (*pFunction)(void);

/**
 * @brief  跳转到外部 Flash 中的应用程序
 * @param  None
 * @retval None
 */
void Bootloader_JumpToApplication(void)
{
    pFunction JumpToApplication;
    uint32_t JumpAddress;
    
    printf("\r\n***************************************\r\n");
    printf("Bootloader: Preparing to jump to external flash application...\r\n");
    
    /* 配置 QSPI 为内存映射模式 */
    if (QSPI_W25Qxx_MemoryMappedMode() != QSPI_W25Qxx_OK)
    {
        printf("ERROR: Failed to configure QSPI memory mapped mode!\r\n");
        return;
    }
    
    printf("QSPI Flash configured in memory mapped mode\r\n");
    
    /* 检查应用程序堆栈指针是否有效 */
    uint32_t sp_value = *(__IO uint32_t*)APP_ADDRESS;
    if ((sp_value & 0xFFFF0000) != 0x24000000 && 
        (sp_value & 0xFFFF0000) != 0x20000000 &&
        (sp_value & 0xFFFF0000) != 0x30000000)
    {
        printf("WARNING: Invalid stack pointer at 0x%08X: 0x%08X\r\n", 
               (unsigned int)APP_ADDRESS, (unsigned int)sp_value);
        printf("Application may not be present in external flash!\r\n");
    }
    
    /* 关闭 ICache 和 DCache */
    SCB_DisableICache();
    SCB_DisableDCache();
    
    /* 关闭 SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* 禁用所有中断 */
    __disable_irq();
    
    /* 读取复位处理函数地址 */
    JumpAddress = *(__IO uint32_t*)(APP_ADDRESS + 4);
    JumpToApplication = (pFunction)JumpAddress;
    
    /* 设置主堆栈指针 */
    __set_MSP(*(__IO uint32_t*)APP_ADDRESS);
    
    printf("Jumping to application at address: 0x%08X\r\n", (unsigned int)JumpAddress);
    printf("Stack pointer set to: 0x%08X\r\n", (unsigned int)sp_value);
    printf("***************************************\r\n\r\n");
    
    /* 跳转到应用程序 */
    JumpToApplication();
    
    /* 不应该执行到这里 */
    while(1)
    {
        printf("ERROR: Failed to jump to application!\r\n");
        HAL_Delay(1000);
    }
}

/**
 * @brief  初始化 Bootloader
 * @param  None
 * @retval None
 */
void Bootloader_Init(void)
{
    printf("\r\n========================================\r\n");
    printf("STM32H750 Bootloader Starting...\r\n");
    printf("========================================\r\n");
    
    /* 初始化 QSPI Flash */
    if (QSPI_W25Qxx_Init() != QSPI_W25Qxx_OK)
    {
        printf("ERROR: QSPI Flash initialization failed!\r\n");
        return;
    }
    
    printf("QSPI Flash W25Q256 initialized successfully\r\n");
    
    /* 读取 Flash ID */
    uint32_t flash_id = QSPI_W25Qxx_ReadID();
    printf("QSPI Flash ID: 0x%06X\r\n", (unsigned int)flash_id);
    
    if (flash_id != W25Qxx_FLASH_ID)
    {
        printf("WARNING: Unexpected Flash ID! Expected: 0x%06X\r\n", 
               (unsigned int)W25Qxx_FLASH_ID);
    }
}
