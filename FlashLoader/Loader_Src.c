/**
  ******************************************************************************
  * @file    Loader_Src.c
  * @author  STM32CubeProgrammer Flash Loader Team
  * @brief   Flash Loader source file for W25Q256 external flash
  ******************************************************************************
  * @attention
  *
  * This file provides the implementation of the Flash Loader interface
  * required by STM32CubeProgrammer for external flash programming.
  * 
  * The Flash Loader must implement the following functions:
  * - Init(): Initialize the external flash
  * - Write(): Write data to flash
  * - SectorErase(): Erase a sector
  * - MassErase(): Erase the entire flash (optional)
  * - Verify(): Verify written data (optional)
  * 
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"

#ifdef FLASH_LOADER
// Flash Loader模式下包含最小依赖
#include "Loader_Src.h"
#include "Dev_Inf.h"
#else
// 正常模式下包含完整驱动
#include "quadspi.h"
#include "qspi_w25q256.h"
#include "debug.h"
#endif

/* Private defines */
#define LOADER_OK            0x1
#define LOADER_FAIL          0x0
#define TIMEOUT_VALUE        1000

/* Flash parameters for W25Q256 */
#define FLASH_BASE_ADDRESS   0x90000000  /* Memory mapped address */
#define FLASH_SIZE           0x2000000   /* 32MB */
#define SECTOR_SIZE          0x1000      /* 4KB sector */
#define PAGE_SIZE            0x100       /* 256 bytes */

/* W25Q256 Command Set */
#define W25Q256_RESET_ENABLE_CMD         0x66
#define W25Q256_RESET_MEMORY_CMD         0x99
#define W25Q256_READ_ID_CMD              0x9F
#define W25Q256_WRITE_ENABLE_CMD         0x06
#define W25Q256_WRITE_DISABLE_CMD        0x04
#define W25Q256_READ_STATUS_REG1_CMD     0x05
#define W25Q256_PAGE_PROG_CMD            0x02
#define W25Q256_QUAD_PAGE_PROG_CMD       0x32
#define W25Q256_SECTOR_ERASE_CMD         0x20
#define W25Q256_BULK_ERASE_CMD           0xC7
#define W25Q256_FAST_READ_CMD            0x0B
#define W25Q256_QUAD_INOUT_READ_CMD      0xEB

/* Status Register bits */
#define W25Q256_SR_WIP                   0x01    /* Write in progress */
#define W25Q256_SR_WEL                   0x02    /* Write enable latch */

/* Expected Flash ID */
#define W25Q256_FLASH_ID                 0xEF4019

/* External variables */
#ifndef FLASH_LOADER
extern QSPI_HandleTypeDef hqspi;
#endif
extern void Error_Handler(void);

#ifdef FLASH_LOADER
/* Flash Loader模式下的QUADSPI全局变量 */
QSPI_HandleTypeDef hqspi;

/* Flash Loader模式下的简化QUADSPI初始化 */
static void FL_QUADSPI_Init(void);
static void FL_QUADSPI_MspInit(void);
#endif

/* Private function prototypes */
static int CSP_QUADSPI_Init(void);
static int CSP_QSPI_WriteEnable(void);
static int CSP_QSPI_AutoPollingMemReady(void);
static int CSP_QSPI_Configuration(void);
static int CSP_QSPI_EnableMemoryMappedMode(void);
static uint32_t CSP_QSPI_ReadID(void);

/**
  * @brief  Initialize the external flash interface
  * @param  None
  * @retval LOADER_OK (1) if success, LOADER_FAIL (0) otherwise
  */
__attribute__((section(".text.Init"), used, externally_visible))
int Init(void)
{
    SystemInit();
    
#ifdef FLASH_LOADER
    /* Flash Loader模式：使用简化的QUADSPI初始化 */
    FL_QUADSPI_Init();
    
    /* 检查Flash ID */
    uint32_t flash_id = CSP_QSPI_ReadID();
    if (flash_id != W25Q256_FLASH_ID) {
        return LOADER_FAIL;
    }
#else
    /* 正常模式：使用完整的QUADSPI初始化 */
    if (CSP_QUADSPI_Init() != 0) {
        return LOADER_FAIL;
    }
    
    if (CSP_QSPI_Configuration() != 0) {
        return LOADER_FAIL;
    }
    
    /* Initialize W25Q256 driver */
    if (QSPI_W25Qxx_Init() != QSPI_W25Qxx_OK) {
        return LOADER_FAIL;
    }
    
    /* Check Flash ID */
    uint32_t flash_id = QSPI_W25Qxx_ReadID();
    if (flash_id != W25Qxx_FLASH_ID) {
        return LOADER_FAIL;
    }
#endif
    
    return LOADER_OK;
}

/**
  * @brief  Write data to the external flash
  * @param  Address: Address to write data to
  * @param  Size: Size of data to write  
  * @param  buffer: Pointer to data buffer
  * @retval LOADER_OK (1) if success, LOADER_FAIL (0) otherwise
  */
__attribute__((section(".text.Write"), used, externally_visible))
int Write(uint32_t Address, uint32_t Size, uint8_t* buffer)
{
    uint32_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
    uint32_t QSPI_DataSize = Size;
    uint8_t* QSPI_pBuffer = buffer;
    uint32_t WriteAddr = Address;
    
    /* Calculate the number of pages and remaining bytes */
    Addr = WriteAddr % PAGE_SIZE;
    count = PAGE_SIZE - Addr;
    NumOfPage = QSPI_DataSize / PAGE_SIZE;
    NumOfSingle = QSPI_DataSize % PAGE_SIZE;
    
    if (Addr == 0) /* WriteAddr is Page aligned */
    {
        if (NumOfPage == 0) /* QSPI_DataSize < PAGE_SIZE */
        {
            if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, QSPI_DataSize) != 0)
                return LOADER_FAIL;
        }
        else /* QSPI_DataSize >= PAGE_SIZE */
        {
            while (NumOfPage--)
            {
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, PAGE_SIZE) != 0)
                    return LOADER_FAIL;
                WriteAddr += PAGE_SIZE;
                QSPI_pBuffer += PAGE_SIZE;
            }
            
            if (NumOfSingle != 0)
            {
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, NumOfSingle) != 0)
                    return LOADER_FAIL;
            }
        }
    }
    else /* WriteAddr is not Page aligned */
    {
        if (NumOfPage == 0) /* QSPI_DataSize < PAGE_SIZE */
        {
            if (NumOfSingle > count) /* (QSPI_DataSize + WriteAddr) > PAGE_SIZE */
            {
                temp = NumOfSingle - count;
                
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, count) != 0)
                    return LOADER_FAIL;
                WriteAddr += count;
                QSPI_pBuffer += count;
                
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, temp) != 0)
                    return LOADER_FAIL;
            }
            else
            {
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, QSPI_DataSize) != 0)
                    return LOADER_FAIL;
            }
        }
        else /* QSPI_DataSize >= PAGE_SIZE */
        {
            QSPI_DataSize -= count;
            NumOfPage = QSPI_DataSize / PAGE_SIZE;
            NumOfSingle = QSPI_DataSize % PAGE_SIZE;
            
            if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, count) != 0)
                return LOADER_FAIL;
            WriteAddr += count;
            QSPI_pBuffer += count;
            
            while (NumOfPage--)
            {
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, PAGE_SIZE) != 0)
                    return LOADER_FAIL;
                WriteAddr += PAGE_SIZE;
                QSPI_pBuffer += PAGE_SIZE;
            }
            
            if (NumOfSingle != 0)
            {
                if (CSP_QSPI_WritePage(QSPI_pBuffer, WriteAddr, NumOfSingle) != 0)
                    return LOADER_FAIL;
            }
        }
    }
    
    return LOADER_OK;
}

/**
  * @brief  Erase a sector of the external flash
  * @param  EraseStartAddress: Start address of sector to erase
  * @param  EraseEndAddress: End address of sector to erase  
  * @retval LOADER_OK (1) if success, LOADER_FAIL (0) otherwise
  */
__attribute__((section(".text.SectorErase"), used, externally_visible))
int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress)
{
    uint32_t BlockAddr;
    
    /* Get the sector address aligned to sector boundary */
    EraseStartAddress = EraseStartAddress - (EraseStartAddress % SECTOR_SIZE);
    
    while (EraseStartAddress <= EraseEndAddress)
    {
        BlockAddr = EraseStartAddress & 0x0FFFFFFF;
        
        if (CSP_QSPI_EraseSector(BlockAddr) != 0)
            return LOADER_FAIL;
        
        EraseStartAddress += SECTOR_SIZE;
    }
    
    return LOADER_OK;
}

/**
  * @brief  Erase the entire external flash (Mass Erase)
  * @param  None
  * @retval LOADER_OK (1) if success, LOADER_FAIL (0) otherwise
  */
__attribute__((section(".text.MassErase"), used, externally_visible))
int MassErase(void)
{
    QSPI_CommandTypeDef sCommand;
    
    /* Enable write operations */
    if (CSP_QSPI_WriteEnable() != 0)
        return LOADER_FAIL;
    
    /* Bulk erase command */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_BULK_ERASE_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return LOADER_FAIL;
    
    /* Wait for write operation to complete */
    if (CSP_QSPI_AutoPollingMemReady() != 0)
        return LOADER_FAIL;
    
    return LOADER_OK;
}

/**
  * @brief  Verify flash content after write operation
  * @param  MemoryAddr: Address to verify
  * @param  RAMBufferAddr: RAM buffer containing expected data
  * @param  BufferSize: Size of data to verify
  * @param  missalignement: Alignment offset
  * @retval LOADER_OK (1) if success, LOADER_FAIL (0) otherwise
  */
uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t BufferSize, uint32_t missalignement)
{
    uint32_t VerifiedData = 0;
    uint8_t TmpBuffer = 0x00;
    uint64_t checksum = 0;
    
    /* Enable memory mapped mode */
    if (CSP_QSPI_EnableMemoryMappedMode() != 0)
        return LOADER_FAIL;
    
    checksum = 0;
    
    for (VerifiedData = 0; VerifiedData < BufferSize; VerifiedData += 4)
    {
        /* Read data from memory mapped mode */
        TmpBuffer = *(__IO uint8_t*)(MemoryAddr + VerifiedData);
        checksum += TmpBuffer;
    }
    
    return checksum;
}

/* Private Functions Implementation */

/**
  * @brief  Initialize the QUADSPI interface
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QUADSPI_Init(void)
{
    /* Call the original initialization function */
    MX_QUADSPI_Init();
    
    /* Reset the W25Q256 flash */
    QSPI_CommandTypeDef sCommand;
    
    /* Enable Reset */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_RESET_ENABLE_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    /* Reset Memory */
    sCommand.Instruction = W25Q256_RESET_MEMORY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    /* Wait for reset to complete */
    HAL_Delay(1);
    
    return 0;
}

/**
  * @brief  Configure QSPI for flash operations
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_Configuration(void)
{
    /* Configuration is done in MX_QUADSPI_Init() */
    return 0;
}

/**
  * @brief  Enable write operations on the flash
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_WriteEnable(void)
{
    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;
    
    /* Enable write operations */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_WRITE_ENABLE_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    /* Configure automatic polling mode to wait for write enable */
    sConfig.Match           = W25Q256_SR_WEL;
    sConfig.Mask            = W25Q256_SR_WEL;
    sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval        = 0x10;
    sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
    
    sCommand.Instruction    = W25Q256_READ_STATUS_REG1_CMD;
    sCommand.DataMode       = QSPI_DATA_1_LINE;
    
    if (HAL_QSPI_AutoPolling(&hqspi, &sCommand, &sConfig, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    return 0;
}

/**
  * @brief  Wait for flash to be ready (not busy)
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_AutoPollingMemReady(void)
{
    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;
    
    /* Configure automatic polling mode to wait for memory ready */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_READ_STATUS_REG1_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    sConfig.Match           = 0;
    sConfig.Mask            = W25Q256_SR_WIP;
    sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval        = 0x10;
    sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
    
    if (HAL_QSPI_AutoPolling(&hqspi, &sCommand, &sConfig, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    return 0;
}

/**
  * @brief  Enable memory mapped mode
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_EnableMemoryMappedMode(void)
{
    QSPI_CommandTypeDef      sCommand;
    QSPI_MemoryMappedTypeDef sMemMappedCfg;
    
    /* Configure the command for the read instruction */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_FAST_READ_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 8;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    /* Configure the memory mapped mode */
    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    sMemMappedCfg.TimeOutPeriod     = 0;
    
    if (HAL_QSPI_MemoryMapped(&hqspi, &sCommand, &sMemMappedCfg) != HAL_OK)
        return -1;
    
    return 0;
}

/**
  * @brief  Write a page (256 bytes max) to the flash
  * @param  buffer: Pointer to data buffer
  * @param  address: Write address
  * @param  size: Size of data to write (max 256 bytes)
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_WritePage(uint8_t* buffer, uint32_t address, uint32_t size)
{
    QSPI_CommandTypeDef sCommand;
    
    /* Enable write operations */
    if (CSP_QSPI_WriteEnable() != 0)
        return -1;
    
    /* Writing Sequence */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_PAGE_PROG_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.Address           = address & 0x0FFFFFFF;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.NbData            = size;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    if (HAL_QSPI_Transmit(&hqspi, buffer, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    /* Configure automatic polling mode to wait for end of program */
    if (CSP_QSPI_AutoPollingMemReady() != 0)
        return -1;
    
    return 0;
}

/**
  * @brief  Erase a sector (4KB) of the flash
  * @param  address: Sector address to erase
  * @retval 0 if success, -1 otherwise
  */
static int CSP_QSPI_EraseSector(uint32_t address)
{
    QSPI_CommandTypeDef sCommand;
    
    /* Enable write operations */
    if (CSP_QSPI_WriteEnable() != 0)
        return -1;
    
    /* Erasing Sequence */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_SECTOR_ERASE_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.Address           = address;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return -1;
    
    /* Configure automatic polling mode to wait for end of erase */
    if (CSP_QSPI_AutoPollingMemReady() != 0)
        return -1;
    
    return 0;
}

/**
  * @brief  Read the flash ID
  * @retval Flash ID value
  */
static uint32_t CSP_QSPI_ReadID(void)
{
    QSPI_CommandTypeDef sCommand;
    uint8_t id[3];
    uint32_t flash_id = 0;
    
    /* Read JEDEC ID */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = W25Q256_READ_ID_CMD;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.NbData            = 3;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    
    if (HAL_QSPI_Command(&hqspi, &sCommand, TIMEOUT_VALUE) != HAL_OK)
        return 0;
    
    if (HAL_QSPI_Receive(&hqspi, id, TIMEOUT_VALUE) != HAL_OK)
        return 0;
    
    flash_id = (id[0] << 16) | (id[1] << 8) | id[2];
    
    return flash_id;
}

#ifdef FLASH_LOADER

/**
  * @brief  简化的QUADSPI初始化（Flash Loader专用）
  * @retval None
  */
static void FL_QUADSPI_Init(void)
{
    /* QUADSPI peripheral initialization */
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 1;
    hqspi.Init.FifoThreshold = 8;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = 24;  // 2^(24+1) = 32MB for W25Q256
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    
    /* MSP initialization */
    FL_QUADSPI_MspInit();
    
    /* Initialize QUADSPI */
    if (HAL_QSPI_Init(&hqspi) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  简化的QUADSPI MSP初始化（Flash Loader专用）
  * @retval None
  */
static void FL_QUADSPI_MspInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    
    /* Peripheral clock enable */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    
    /**QUADSPI GPIO Configuration
    PG6     ------> QUADSPI_BK1_NCS
    PF6     ------> QUADSPI_BK1_IO3
    PF7     ------> QUADSPI_BK1_IO2
    PF8     ------> QUADSPI_BK1_IO0
    PF10    ------> QUADSPI_CLK
    PF9     ------> QUADSPI_BK1_IO1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

#endif /* FLASH_LOADER */