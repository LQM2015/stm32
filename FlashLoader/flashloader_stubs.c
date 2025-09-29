/**
 * @file    flashloader_stubs.c
 * @brief   Flash Loader需要的桩函数
 * @author  Assistant
 * @date    2025-09-29
 */

#include "main.h"

/**
 * @brief  Assert失败处理函数（桩函数）
 * @param  file: 源文件名
 * @param  line: 行号  
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* Flash Loader中不使用assert，这里提供空实现 */
    (void)file;
    (void)line;
    /* 在Flash Loader中可以选择忽略assert或者进入死循环 */
    // while(1) { }
}

/**
 * @brief  MX_QUADSPI_Init桩函数
 * @note   Flash Loader使用自己的QSPI初始化，不需要CubeMX生成的初始化
 * @retval None
 */
void MX_QUADSPI_Init(void)
{
    /* Flash Loader有自己的QSPI初始化代码 */
    /* 这里提供空实现，避免链接错误 */
}

/**
 * @brief  HAL_MDMA相关桩函数
 * @note   Flash Loader通常不使用DMA，提供空实现
 */
HAL_StatusTypeDef HAL_MDMA_Start_IT(MDMA_HandleTypeDef *hmdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t BlockDataLength, uint32_t BlockCount)
{
    (void)hmdma; (void)SrcAddress; (void)DstAddress; (void)BlockDataLength; (void)BlockCount;
    return HAL_ERROR; /* Flash Loader不使用MDMA */
}

HAL_StatusTypeDef HAL_MDMA_Abort(MDMA_HandleTypeDef *hmdma)
{
    (void)hmdma;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_MDMA_Abort_IT(MDMA_HandleTypeDef *hmdma)
{
    (void)hmdma;
    return HAL_OK;
}