/**
 ******************************************************************************
 * @file    bootloader.h
 * @author  Your Name
 * @brief   Header for bootloader.c file
 * @details STM32H750 Bootloader functionality
 *          - Initialize QSPI Flash
 *          - Configure system for external Flash execution
 *          - Jump to application code
 ******************************************************************************
 */

#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_hal.h"

/* Exported types ------------------------------------------------------------*/
typedef void (*pFunction)(void);

/* Exported constants --------------------------------------------------------*/
/**
 * @brief External Flash base address (QSPI memory-mapped mode)
 */
#define APP_ADDRESS         0x90000000U

/**
 * @brief Application start offset (bytes from Flash base)
 */
#define APP_OFFSET          0x00000000U

/**
 * @brief Maximum application size (32MB - leave space for data)
 */
#define APP_MAX_SIZE        0x01000000U  /* 16MB for code */

/**
 * @brief Bootloader version
 */
#define BOOTLOADER_VERSION  "1.0.0"

/* Exported macro ------------------------------------------------------------*/
/**
 * @brief Get application reset vector
 */
#define APP_RESET_VECTOR()  (*(__IO uint32_t*)(APP_ADDRESS + APP_OFFSET + 4))

/**
 * @brief Get application stack pointer
 */
#define APP_STACK_POINTER() (*(__IO uint32_t*)(APP_ADDRESS + APP_OFFSET))

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize bootloader and prepare for application jump
 * @retval None
 */
void Bootloader_Init(void);

/**
 * @brief  Verify application is valid and ready to run
 * @retval 1 if application is valid, 0 otherwise
 */
uint8_t Bootloader_VerifyApplication(void);

/**
 * @brief  Jump to application code in external Flash
 * @note   This function does not return
 * @retval None
 */
void Bootloader_JumpToApplication(void) __attribute__((noreturn));

/**
 * @brief  Deinitialize peripherals before jumping
 * @retval None
 */
void Bootloader_DeInit(void);

/**
 * @brief  Print bootloader information
 * @retval None
 */
void Bootloader_PrintInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */
