#ifndef QSPI_W25Q256_H
#define QSPI_W25Q256_H
#include "stm32h7xx_hal.h"
#include "debug.h"



/*----------------------------------------------- Error Codes -------------------------------------------*/



#define QSPI_W25Qxx_OK                  0       // W25Qxx Communication Success

#define W25Qxx_ERROR_INIT               -1      // Initialization Error

#define W25Qxx_ERROR_WriteEnable        -2      // Write Enable Error

#define W25Qxx_ERROR_AUTOPOLLING        -3      // Auto-polling Wait Error

#define W25Qxx_ERROR_Erase              -4      // Erase Error

#define W25Qxx_ERROR_TRANSMIT           -5      // Transmission Error

#define W25Qxx_ERROR_MemoryMapped       -6      // Memory Mapped Mode Error



/* W25Q256 Command Set */

#define W25Qxx_CMD_EnableReset          0x66    // Enable Reset

#define W25Qxx_CMD_ResetDevice          0x99    // Reset Device 

#define W25Qxx_CMD_JedecID              0x9F    // JEDEC ID  

#define W25Qxx_CMD_WriteEnable          0x06    // Write Enable

#define W25Qxx_CMD_SectorErase          0x21    // Sector Erase (4K bytes), Reference time: 45ms

#define W25Qxx_CMD_BlockErase_64K       0xDC    // Block Erase (64K bytes), Reference time: 150ms

#define W25Qxx_CMD_ChipErase            0xC7    // Chip Erase, Reference time: 80S

#define W25Qxx_CMD_QuadInputPageProgram 0x34    // 1-1-4 mode Page Program, Reference time: 0.4ms

#define W25Qxx_CMD_FastReadQuad_IO      0xEC    // 1-4-4 mode Fast Read Quad I/O

#define W25Qxx_CMD_ReadStatus_REG1      0x05    // Read Status Register 1

#define W25Qxx_Status_REG1_BUSY         0x01    // Busy flag in Status Register 1

#define W25Qxx_Status_REG1_WEL          0x02    // Write Enable Latch in Status Register 1

/* W25Q256 Specifications */

#define W25Qxx_PageSize                 256         // Page size: 256 bytes

#define W25Qxx_FlashSize                0x2000000   // W25Q256 size: 32M bytes (256Mbit)

#define W25Qxx_ChipErase_TIMEOUT_MAX    400000U     // Timeout: W25Q256 chip erase max time 400S

#define W25Qxx_FLASH_ID                 0xef4019    // W25Q256 JEDEC ID
#define W25Qxx_Mem_Addr                 0x90000000  // Memory mapped mode address


/*----------------------------------------------- Function Prototypes ---------------------------------------------------*/

int8_t   QSPI_W25Qxx_Init(void);                   // W25Qxx Initialization

int8_t   QSPI_W25Qxx_Reset(void);                  // Reset Device

uint32_t QSPI_W25Qxx_ReadID(void);                 // Read Device ID

int8_t   QSPI_W25Qxx_Test(void);                   // Flash Read/Write Test	

int8_t   QSPI_W25Qxx_MemoryMappedMode(void);       // Enable Memory Mapped Mode

int8_t   QSPI_W25Qxx_SectorErase(uint32_t SectorAddress);      // Sector Erase (4K bytes)

int8_t   QSPI_W25Qxx_BlockErase_64K(uint32_t SectorAddress);   // Block Erase (64K bytes)

int8_t   QSPI_W25Qxx_ChipErase(void);                          // Chip Erase



/* Read/Write Functions */

int8_t   QSPI_W25Qxx_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);    // Page Write (max 256 bytes)#endif // QSPI_w25q64_H 

int8_t   QSPI_W25Qxx_WriteBuffer(uint8_t* pData, uint32_t WriteAddr, uint32_t Size);              // Buffer Write (any size)

int8_t   QSPI_W25Qxx_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);     // Buffer Read (any size)



/* Advanced Functions */

int8_t   QSPI_W25Qxx_WriteEnable(void);                        // Write Enable

int8_t   QSPI_W25Qxx_AutoPollingMemReady(void);               // Auto-polling Memory Ready
const char* QSPI_W25Qxx_GetErrorString(int8_t error_code);     // Get Error String

/* Utility Functions */
uint32_t QSPI_W25Qxx_GetSectorAddress(uint32_t address);       // Get sector start address  
uint32_t QSPI_W25Qxx_GetBlockAddress(uint32_t address);        // Get block start address
uint8_t  QSPI_W25Qxx_IsSectorEmpty(uint32_t sector_addr);      // Check if sector is empty
void     QSPI_W25Qxx_PrintInfo(void);                          // Print Flash information

#endif // QSPI_W25Q256_H
