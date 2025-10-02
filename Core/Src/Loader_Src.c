/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include "qspi_w25q256.h"
#include <string.h>

/* External variables --------------------------------------------------------*/
QSPI_HandleTypeDef hqspi;  // Define QSPI handle for external loader

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef QSPI_LoaderWait(uint32_t timeout_ms);
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_QUADSPI_Init_Loader(void);

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化函数 - External Loader Entry Point
  * @retval int: 0表示失败，1表示成功
  */
int Init(void)
{
    // HAL库初始化
    HAL_Init();
    
    // 配置系统时钟
    SystemClock_Config();
    
    // 初始化GPIO
    MX_GPIO_Init();
    
    // 初始化QSPI
    MX_QUADSPI_Init_Loader();

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

/**
  * @brief System Clock Configuration for External Loader
  * @retval None
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Supply configuration update enable
    */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;      // 25MHz / 5 = 5MHz
    RCC_OscInitStruct.PLL.PLLN = 96;     // 5MHz * 96 = 480MHz
    RCC_OscInitStruct.PLL.PLLP = 2;      // 480MHz / 2 = 240MHz (SYSCLK)
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while(1);
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;    // 240MHz / 2 = 120MHz
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;   // 120MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;   // 60MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;   // 60MHz
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;   // 60MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        while(1);
    }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
}

/**
  * @brief QUADSPI Initialization Function for Loader
  * @param None
  * @retval None
  */
static void MX_QUADSPI_Init_Loader(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Initializes the peripherals clock
    */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        while(1);
    }

    /* QUADSPI clock enable */
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

    /* QUADSPI initialization */
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 1;
    hqspi.Init.FifoThreshold = 4;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = 24;                             // 2^(24+1) = 32MB
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    
    if (HAL_QSPI_Init(&hqspi) != HAL_OK)
    {
        while(1);
    }
}

/* USER CODE END 1 */

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* Infinite loop for debugging */
    while (1)
    {
    }
}
#endif /* USE_FULL_ASSERT */