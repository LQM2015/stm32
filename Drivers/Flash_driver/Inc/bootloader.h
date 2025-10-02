#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/*----------------------------------------------- 相关宏定义 -------------------------------------------*/

// 外部 QSPI Flash 的内存映射地址
#define W25Qxx_Mem_Addr    0x90000000

// 应用程序在外部 Flash 中的起始地址
#define APP_ADDRESS        W25Qxx_Mem_Addr

/*----------------------------------------------- 函数声明 -------------------------------------------*/

/**
 * @brief  跳转到外部 Flash 中的应用程序
 * @param  None
 * @retval None
 */
void Bootloader_JumpToApplication(void);

/**
 * @brief  初始化 Bootloader
 * @param  None
 * @retval None
 */
void Bootloader_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */
