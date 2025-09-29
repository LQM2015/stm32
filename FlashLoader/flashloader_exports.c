/**
 * @file    flashloader_exports.c
 * @brief   强制导出Flash Loader API函数
 * @author  Assistant
 * @date    2025-09-29
 */

#include <stdint.h>

/* 强制导出Flash Loader API函数，确保STM32CubeProgrammer能找到它们 */

/* 外部声明Flash Loader API函数 */
extern int Init(void);
extern int Write(uint32_t Address, uint32_t Size, uint8_t* buffer);  
extern int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress);
extern int MassErase(void);
extern uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t NumBytes, uint32_t missalignement);

/* 强制引用所有API函数，防止被链接器优化掉 */
__attribute__((section(".text"), used, externally_visible))
void* FlashLoaderAPITable[] = {
    (void*)Init,
    (void*)Write,
    (void*)SectorErase, 
    (void*)MassErase,
    (void*)Verify
};

/* 导出函数计数 */
__attribute__((section(".rodata"), used, externally_visible))
const int FlashLoaderAPICount = 5;