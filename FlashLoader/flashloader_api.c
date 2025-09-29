/**
 * @file    flashloader_api.c
 * @brief   Flash Loader API函数引用表 - 防止链接器优化掉API函数
 * @author  Assistant
 * @date    2025-09-29
 */

#include "main.h"

/* Flash Loader API函数声明 */
extern int Init(void);
extern int Write(uint32_t Address, uint32_t Size, uint8_t* buffer);
extern int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress);
extern int MassErase(void);

/**
 * @brief Flash Loader API函数引用表
 * @note  这个数组确保链接器不会优化掉Flash Loader API函数
 *        STM32CubeProgrammer会通过符号表查找这些函数的地址
 */
__attribute__((section(".flashloader_api"), used))
const void* FlashLoaderAPI[] = {
    (void*)Init,
    (void*)Write, 
    (void*)SectorErase,
    (void*)MassErase
};