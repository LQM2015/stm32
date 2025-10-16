/**
 ******************************************************************************
 * @file    bsp_sdram.h
 * @author  Your Name
 * @brief   SDRAM驱动头文件
 * @details 提供SDRAM初始化和测试功能
 ******************************************************************************
 */

#ifndef __BSP_SDRAM_H
#define __BSP_SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include <stdio.h>

/* Exported defines ----------------------------------------------------------*/
#define SDRAM_SIZE              (32 * 1024 * 1024)      // 32MB (256Mbit) - IS42S32800J: 12位行地址 × 9位列地址 × 32位宽
#define SDRAM_BANK_ADDR         ((uint32_t)0xC0000000) // FMC SDRAM 数据基地址
#define FMC_COMMAND_TARGET_BANK FMC_SDRAM_CMD_TARGET_BANK1  // SDRAM 的bank选择
#define SDRAM_TIMEOUT_VALUE     ((uint32_t)0x1000)      // 超时判断时间

/* SDRAM模式寄存器定义 */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/* Exported types ------------------------------------------------------------*/
typedef enum {
    SDRAM_OK       = 0,
    SDRAM_ERROR    = 1,
    SDRAM_TIMEOUT  = 2
} BSP_SDRAM_StatusTypeDef;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  SDRAM初始化序列
 * @param  hsdram: SDRAM_HandleTypeDef结构体变量指针
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram);

/**
 * @brief  SDRAM读写测试
 * @param  None
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_Test(void);

/**
 * @brief  SDRAM性能测试
 * @param  None
 * @retval None
 */
void BSP_SDRAM_Performance_Test(void);

/**
 * @brief  写入数据到SDRAM
 * @param  address: 写入地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData(uint32_t address, uint8_t *pData, uint32_t size);

/**
 * @brief  从SDRAM读取数据
 * @param  address: 读取地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData(uint32_t address, uint8_t *pData, uint32_t size);

/**
 * @brief  使用DMA写入数据到SDRAM（高速传输）
 * @param  address: 写入地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData_DMA(uint32_t address, uint8_t *pData, uint32_t size);

/**
 * @brief  使用DMA从SDRAM读取数据（高速传输）
 * @param  address: 读取地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData_DMA(uint32_t address, uint8_t *pData, uint32_t size);

/**
 * @brief  SDRAM DMA传输完成回调函数
 * @param  None
 * @retval None
 */
void BSP_SDRAM_DMA_TransferCompleteCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SDRAM_H */
