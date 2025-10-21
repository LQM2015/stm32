#include "sdram_loader.h"
#include "fmc.h"
#include "bsp_sdram.h"
#include "mdma.h"
#include <string.h>
#include "stm32h7xx.h"
#include "shell_log.h"  /* APP uses SHELL_LOG_MEMORY_xxx macros */

static HAL_StatusTypeDef mdma_copy_blocks(uint32_t src, uint32_t dst, uint32_t size_bytes)
{
  MDMA_HandleTypeDef hmdma;
  hmdma.Instance = MDMA_Channel2; /* 选择一个未被其他BSP占用的通道 */
  hmdma.Init.Request = MDMA_REQUEST_SW;
  hmdma.Init.TransferTriggerMode = MDMA_BLOCK_TRANSFER;
  hmdma.Init.Priority = MDMA_PRIORITY_HIGH;
  hmdma.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
  hmdma.Init.SourceInc = MDMA_SRC_INC_WORD;
  hmdma.Init.DestinationInc = MDMA_DEST_INC_WORD;
  hmdma.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
  hmdma.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
  hmdma.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
  hmdma.Init.BufferTransferLength = 128; /* 128-beat缓冲，提高突发效率 */
  hmdma.Init.SourceBurst = MDMA_SOURCE_BURST_32BEATS;
  hmdma.Init.DestBurst = MDMA_DEST_BURST_32BEATS;
  if (HAL_MDMA_Init(&hmdma) != HAL_OK) {
    SHELL_LOG_MEMORY_ERROR("MDMA init failed for SDRAM loader");
    return HAL_ERROR;
  }

  HAL_StatusTypeDef st = HAL_OK;
  const uint32_t chunk = 128 * 1024; /* 128KB分块搬运 */
  uint32_t copied = 0;
  /* 目的区域先失效D-Cache，避免读取到旧缓存行 */
  uint32_t inv_len = (size_bytes + 31U) & ~31U;
  SCB_InvalidateDCache_by_Addr((uint32_t*)dst, inv_len);
  __DSB();
  while (copied < size_bytes && st == HAL_OK) {
    uint32_t this_sz = size_bytes - copied;
    if (this_sz > chunk) this_sz = chunk;
    uint32_t this_aligned = (this_sz + 31U) & ~31U;
    st = HAL_MDMA_Start(&hmdma, src + copied, dst + copied, this_aligned, 1);
    if (st != HAL_OK) break;
    st = HAL_MDMA_PollForTransfer(&hmdma, HAL_MDMA_FULL_TRANSFER, 1000);
    if (st != HAL_OK) break;
    copied += this_aligned;
  }
  return st;
}

void SDRAM_InitAndLoadSections(void)
{
  /* 关闭中断以避免在FMC初始化过程中可能的取指/中断问题 */
  __disable_irq();
  MX_FMC_Init();
  __enable_irq();
  SHELL_LOG_MEMORY_INFO("FMC initialized by SDRAM loader");

  if (BSP_SDRAM_Initialization_Sequence(&hsdram1) != SDRAM_OK) {
    SHELL_LOG_MEMORY_ERROR("SDRAM initialization failed in loader");
    return;
  }
  SHELL_LOG_MEMORY_INFO("SDRAM init OK. Base: 0x%08X, Size: %d MB", SDRAM_BANK_ADDR, SDRAM_SIZE/1024/1024);

  /* 从链接脚本导入拷贝符号 */
  extern uint32_t __sdram_text_load, __sdram_text_start, __sdram_text_end;
  extern uint32_t __sdram_rodata_load, __sdram_rodata_start, __sdram_rodata_end;
  uint32_t text_size   = (uint32_t)&__sdram_text_end   - (uint32_t)&__sdram_text_start;
  uint32_t rodata_size = (uint32_t)&__sdram_rodata_end - (uint32_t)&__sdram_rodata_start;

  /* 使用MDMA进行搬运 */
  if (text_size) {
    if (mdma_copy_blocks((uint32_t)&__sdram_text_load, (uint32_t)&__sdram_text_start, text_size) != HAL_OK) {
      SHELL_LOG_MEMORY_ERROR("MDMA copy .text_sdram failed");
    } else {
      SHELL_LOG_MEMORY_INFO("MDMA copied .text_sdram: %lu bytes", text_size);
    }
  }
  if (rodata_size) {
    if (mdma_copy_blocks((uint32_t)&__sdram_rodata_load, (uint32_t)&__sdram_rodata_start, rodata_size) != HAL_OK) {
      SHELL_LOG_MEMORY_ERROR("MDMA copy .rodata_sdram failed");
    } else {
      SHELL_LOG_MEMORY_INFO("MDMA copied .rodata_sdram: %lu bytes", rodata_size);
    }
  }

  /* 失效目的区域D-Cache，随后失效I-Cache，确保取指从SDRAM */
  if (text_size) {
    uint32_t start = (uint32_t)&__sdram_text_start;
    uint32_t inv_len = (text_size + 31U) & ~31U;
    SCB_InvalidateDCache_by_Addr((uint32_t*)start, inv_len);
  }
  if (rodata_size) {
    uint32_t start = (uint32_t)&__sdram_rodata_start;
    uint32_t inv_len = (rodata_size + 31U) & ~31U;
    SCB_InvalidateDCache_by_Addr((uint32_t*)start, inv_len);
  }
  __DSB();
  SCB_InvalidateICache();
  __DSB();
  __ISB();
  SHELL_LOG_MEMORY_INFO("SDRAM loader completed. Caches updated");
}