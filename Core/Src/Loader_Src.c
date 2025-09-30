/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "quadspi.h"
#include "gpio.h"
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef QSPI_LoaderWait(uint32_t timeout_ms);
void SystemClock_Config(void);

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  应用程序入口
  * @retval int
  */
int main(void)
{
    // HAL库初始化
    HAL_Init();
    
    // 配置系统时钟
    SystemClock_Config();
    
    // 初始化所有外设
    MX_GPIO_Init();
    MX_QUADSPI_Init();

    // 初始化外部QSPI Flash
    if (QSPI_W25Qxx_Init() != QSPI_W25Qxx_OK)
    {
        Error_Handler();
    }
    
    // 进入主程序
    while (1)
    {
        // 用户代码区
    }
}

/**
  * @brief  初始化函数
  * @retval int: 0表示失败，1表示成功
  */
int Init(void)
{
    // 复位之前的状态
    __set_PRIMASK(0);  // 使能中断
    
    // 初始化HAL库
    HAL_Init();
    
    // 配置系统时钟
    SystemClock_Config();
    
    // 初始化GPIO
    MX_GPIO_Init();
    
    // 初始化QSPI
    MX_QUADSPI_Init();

    // 初始化W25Q256驱动
    if (QSPI_W25Qxx_Init() != QSPI_W25Qxx_OK)
    {
        return 0;
    }

    // 检查Flash ID
    uint32_t id = QSPI_W25Qxx_ReadID();
    if (id != W25Qxx_FLASH_ID)
    {
        return 0;  // 失败返回0
    }
    
    return 1;  // 成功返回1
}

/**
  * @brief  写入函数
  * @param  Address: 写入地址
  * @param  Size: 写入大小
  * @param  buffer: 数据缓冲区
  * @retval int: 0表示失败，1表示成功
  */
int Write(uint32_t Address, uint32_t Size, uint8_t* buffer)
{
    if (Size == 0)
    {
        return 1;
    }

    if (QSPI_W25Qxx_WriteBuffer(buffer, Address, Size) != QSPI_W25Qxx_OK)
    {
        return 0;
    }

    if (QSPI_LoaderWait(5000) != HAL_OK)
    {
        return 0;
    }

    return 1;  // 成功返回1
}

/**
  * @brief  读取函数
  * @param  Address: 读取地址
  * @param  Size: 读取大小
  * @param  buffer: 数据缓冲区
  * @retval int: 0表示失败，1表示成功
  */
int Read(uint32_t Address, uint32_t Size, uint8_t* buffer)
{
    if (QSPI_W25Qxx_ReadBuffer(buffer, Address, Size) != QSPI_W25Qxx_OK)
    {
        return 0;  // 失败返回0
    }
    return 1;  // 成功返回1
}

/**
  * @brief  扇区擦除函数
  * @param  EraseStartAddress: 擦除开始地址
  * @param  EraseEndAddress: 擦除结束地址
  * @retval int: 0表示失败，1表示成功
  */
int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress)
{
    uint32_t sector_size = 0x1000;  // 4KB扇区大小
    uint32_t current_addr = EraseStartAddress;
    
    // 确保地址对齐到扇区
    current_addr = (current_addr / sector_size) * sector_size;
    
    while (current_addr < EraseEndAddress)
    {
        // 擦除4KB扇区
        if (QSPI_W25Qxx_SectorErase(current_addr) != QSPI_W25Qxx_OK)
        {
            return 0;  // 失败返回0
        }

        // 等待擦除完成
        if (QSPI_LoaderWait(3000) != HAL_OK)
        {
            return 0;
        }

        current_addr += sector_size;
    }
    
    return 1;  // 成功返回1
}

/**
  * @brief  整片擦除函数
  * @retval int: 0表示失败，1表示成功
  */
int MassErase(void)
{
    // 执行芯片擦除
    if (QSPI_W25Qxx_ChipErase() != QSPI_W25Qxx_OK)
    {
        return 0;  // 失败返回0
    }

    // 等待擦除完成（芯片擦除时间较长）
    if (QSPI_LoaderWait(200000) != HAL_OK)
    {
        return 0;
    }
    
    return 1;  // 成功返回1
}

/**
  * @brief  校验和计算函数
  * @param  StartAddress: 起始地址
  * @param  Size: 数据大小
  * @param  InitVal: 初始化值
  * @retval uint32_t: 计算得到的校验和
  */
uint32_t CheckSum(uint32_t StartAddress, uint32_t Size, uint32_t InitVal)
{
    uint8_t buffer[256];
    uint32_t checksum = InitVal;
    uint32_t remaining = Size;
    uint32_t current_addr = StartAddress;

    while (remaining > 0)
    {
        uint32_t read_size = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;

        if (QSPI_W25Qxx_ReadBuffer(buffer, current_addr, read_size) != QSPI_W25Qxx_OK)
        {
            return checksum;  // 读取失败，返回当前校验和
        }

        // 简单累加校验
        for (uint32_t i = 0; i < read_size; i++)
        {
            checksum += buffer[i];
        }

        current_addr += read_size;
        remaining -= read_size;
    }

    return checksum;
}

/**
  * @brief  验证函数
  * @param  MemoryAddr: 存储器地址
  * @param  RAMBufferAddr: RAM缓冲区地址
  * @param  Size: 数据大小
  * @param  missalignement: 对齐方式
  * @retval uint64_t: 验证结果，错误时返回错误地址
  */
uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t Size, uint32_t missalignement)
{
    uint32_t verified_data = 0;
    uint8_t read_buffer[256];
    uint8_t* ram_buffer = (uint8_t*)RAMBufferAddr;
    
    while (verified_data < Size) {
        uint32_t read_size = (Size - verified_data > sizeof(read_buffer)) ? 
                             sizeof(read_buffer) : (Size - verified_data);
        
        // 从Flash读取数据
        if (Read(MemoryAddr + verified_data, read_size, read_buffer) != 1) {
            return (MemoryAddr + verified_data) | 0x0100000000000000ULL;  // 错误标志
        }
        
        // 比较数据
        if (memcmp(read_buffer, ram_buffer + verified_data, read_size) != 0) {
            // 找到第一个不匹配的字节
            for (uint32_t i = 0; i < read_size; i++) {
                if (read_buffer[i] != ram_buffer[verified_data + i]) {
                    return MemoryAddr + verified_data + i;  // 返回错误地址
                }
            }
        }
        
        verified_data += read_size;
    }
    
    return MemoryAddr + Size;  // 验证成功，返回结束地址
}

/* USER CODE BEGIN 1 */

static HAL_StatusTypeDef QSPI_LoaderWait(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (QSPI_W25Qxx_AutoPollingMemReady() == QSPI_W25Qxx_OK)
        {
            return HAL_OK;
        }
    }

    return HAL_TIMEOUT;
}

/* USER CODE END 1 */