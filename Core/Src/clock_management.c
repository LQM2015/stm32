#include "clock_management.h"
#include "shell_log.h"
#include "cmsis_os.h"
#include <stdio.h>

/* External HAL Handle defined in main.c */
extern UART_HandleTypeDef huart3;

/**
  * @brief  Simple delay function that doesn't rely on interrupts
  * @param  cycles Number of CPU cycles to wait
  * @retval None
  */
static void DelayNoCycles(uint32_t cycles)
{
    volatile uint32_t i;
    for(i = 0; i < cycles; i++)
    {
        __NOP();
    }
}

// Forward declarations
static ClockProfile_t FindCompatibleProfile(ClockProfile_t requested_profile);

/**
  * @brief  Check if peripheral clocks are compatible with target frequency
  * @param  target_freq Target system clock frequency in Hz
  * @retval 0 if compatible, error code if not compatible
  */
static uint32_t CheckPeripheralCompatibility(uint32_t target_freq)
{
    uint32_t hclk = target_freq / 2;  // AHB clock = SYSCLK / 2
    uint32_t pclk1 = hclk / 2;        // APB1 clock = HCLK / 2
    // uint32_t pclk2 = hclk / 2;        // APB2 clock = HCLK / 2 (未使用)
    uint32_t error_code = 0;
    
    // Check UART3 compatibility (APB1 peripheral)
    // UART3 needs minimum 1MHz for 115200 baud rate
    if (pclk1 < 1000000)
    {
        SHELL_LOG_CLK_ERROR("UART3 incompatible: APB1 clock %lu Hz too low for 115200 baud", pclk1);
        error_code |= 0x01;
    }
    
    // Check SD card compatibility (AHB peripheral)
    // SD card needs minimum 400kHz for initialization, 25MHz for high speed
    if (hclk < 400000)
    {
        SHELL_LOG_CLK_ERROR("SD Card incompatible: AHB clock %lu Hz too low", hclk);
        error_code |= 0x02;
    }
    
    // Check FreeRTOS tick compatibility
    // FreeRTOS needs minimum 1kHz tick rate
    if (target_freq < 1000)
    {
        SHELL_LOG_CLK_ERROR("FreeRTOS incompatible: System clock %lu Hz too low for 1kHz tick", target_freq);
        error_code |= 0x04;
    }
    
    // Check Flash access compatibility
    // Very low frequencies might have Flash access issues
    if (target_freq < 32000 && target_freq != 32768) // Allow 32kHz LSI
    {
        SHELL_LOG_CLK_ERROR("Flash access incompatible: System clock %lu Hz too low", target_freq);
        error_code |= 0x08;
    }
    
    return error_code;
}

/**
  * @brief  Test all clock profiles for compatibility with random switching
  * @retval None
  */
void TestAllClockProfiles(void)
{
    SHELL_LOG_CLK_INFO("=== Random Clock Profile Test ===");
    
    ClockProfile_t profiles[] = {
        CLOCK_PROFILE_32K, CLOCK_PROFILE_24M, CLOCK_PROFILE_48M,
        CLOCK_PROFILE_96M, CLOCK_PROFILE_128M, CLOCK_PROFILE_200M,
        CLOCK_PROFILE_300M, CLOCK_PROFILE_400M, CLOCK_PROFILE_550M
    };
    
    const char* profile_names[] = {
        "32K", "24M", "48M", "96M", "128M", "200M", "300M", "400M", "550M"
    };
    
    // 使用系统tick作为随机种子
    uint32_t seed = HAL_GetTick();
    
    // 测试20次随机切换
    for (int test_count = 0; test_count < 20; test_count++)
    {
        // 简单的线性同余随机数生成器
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        int random_index = seed % 9;
        
        SHELL_LOG_CLK_INFO("--- Random Test %d: Testing %s Profile ---", 
               test_count + 1, profile_names[random_index]);
        
        HAL_StatusTypeDef result = SwitchSystemClock(profiles[random_index]);
        if (result == HAL_OK)
        {
            // 读取当前实际主频
            uint32_t actual_freq = HAL_RCC_GetSysClockFreq();
            SHELL_LOG_CLK_INFO("SUCCESS: %s profile works correctly", profile_names[random_index]);
            SHELL_LOG_CLK_INFO("Actual system clock: %lu Hz (%.2f MHz)", 
                   actual_freq, (float)actual_freq / 1000000.0f);
            
            // 等待5秒 - 现在SysTick已经重新配置，osDelay准确
            SHELL_LOG_CLK_INFO("Waiting 5 seconds before next test...");
            osDelay(5000);
        }
        else
        {
            SHELL_LOG_CLK_ERROR("%s profile failed", profile_names[random_index]);
            osDelay(1000); // 失败时等待1秒
        }
    }
    
    SHELL_LOG_CLK_INFO("=== Random Test Complete ===");
}

/**
  * @brief  Get frequency for a given profile
  * @param  profile Clock profile
  * @retval Frequency in Hz
  */
static uint32_t GetProfileFrequency(ClockProfile_t profile)
{
    switch(profile)
    {
        case CLOCK_PROFILE_32K:  return 32768;
        case CLOCK_PROFILE_24M:  return 24000000;
        case CLOCK_PROFILE_48M:  return 48000000;
        case CLOCK_PROFILE_96M:  return 96000000;
        case CLOCK_PROFILE_128M: return 128000000;
        case CLOCK_PROFILE_200M: return 200000000;
        case CLOCK_PROFILE_300M: return 300000000;
        case CLOCK_PROFILE_400M: return 400000000;
        case CLOCK_PROFILE_550M: return 550000000;
        default: return 24000000;
    }
}

/**
  * @brief  Switches the system clock between pre-defined profiles with compatibility check.
  * @note   Supports 9 different clock frequencies with automatic compatibility checking.
  * @param  profile The target clock profile.
  * @retval HAL_OK if successful, HAL_ERROR if failed
  */
HAL_StatusTypeDef SwitchSystemClock(ClockProfile_t profile)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    uint32_t flash_latency;
    uint32_t pwr_vos_level;
    uint32_t pll_m, pll_n, pll_p;
    uint8_t use_pll = 1;
    uint32_t target_freq;
    
    // Get target frequency
    target_freq = GetProfileFrequency(profile);
    
    SHELL_LOG_CLK_INFO("Requesting clock switch to %lu Hz (Profile %d)", target_freq, profile);
    
    // Check peripheral compatibility
    uint32_t compat_error = CheckPeripheralCompatibility(target_freq);
    if (compat_error != 0)
    {
        SHELL_LOG_CLK_ERROR("Peripheral compatibility check failed (0x%02lX)", compat_error);
        
        // Find compatible alternative
        ClockProfile_t compatible_profile = FindCompatibleProfile(profile);
        if (compatible_profile == profile)
        {
            SHELL_LOG_CLK_ERROR("No compatible clock profile found");
            return HAL_ERROR;
        }
        
        // Use compatible profile instead
        profile = compatible_profile;
        target_freq = GetProfileFrequency(profile);
        SHELL_LOG_CLK_INFO("Using compatible profile %d (%lu Hz)", profile, target_freq);
    }

    // --- Determine target settings based on profile ---
    switch(profile)
    {
        case CLOCK_PROFILE_32K:
            // 32kHz using LSI
            flash_latency = FLASH_LATENCY_0;
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE3;
            use_pll = 0;
            break;
            
        case CLOCK_PROFILE_24M:
            // 24MHz using PLL: 25MHz HSE / 25 * 192 / 8 = 24MHz
            // VCO = 25MHz / 25 * 192 = 192MHz (符合150-836MHz要求)
            // PLLP = 8 (符合STM32H7的1,2,4,6,8,16,128限制)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE3;
            pll_m = 25;
            pll_n = 192;
            pll_p = 8;
            flash_latency = FLASH_LATENCY_0;
            break;
            
        case CLOCK_PROFILE_48M:
            // 48MHz using PLL (25MHz HSE / 5 * 48 / 5 = 48MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE3;
            pll_m = 5;
            pll_n = 48;
            pll_p = 5;
            flash_latency = FLASH_LATENCY_0;
            break;
            
        case CLOCK_PROFILE_96M:
            // 96MHz using PLL (25MHz HSE / 5 * 96 / 5 = 96MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE2;
            pll_m = 5;
            pll_n = 96;
            pll_p = 5;
            flash_latency = FLASH_LATENCY_1;
            break;
            
        case CLOCK_PROFILE_128M:
            // 128MHz using PLL (25MHz HSE / 5 * 128 / 5 = 128MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE2;
            pll_m = 5;
            pll_n = 128;
            pll_p = 5;
            flash_latency = FLASH_LATENCY_1;
            break;
            
        case CLOCK_PROFILE_200M:
            // 200MHz using PLL (25MHz HSE / 5 * 80 / 2 = 200MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE1;
            pll_m = 5;
            pll_n = 80;
            pll_p = 2;
            flash_latency = FLASH_LATENCY_2;
            break;
            
        case CLOCK_PROFILE_300M:
            // 300MHz using PLL (25MHz HSE / 5 * 120 / 2 = 300MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE1;
            pll_m = 5;
            pll_n = 120;
            pll_p = 2;
            flash_latency = FLASH_LATENCY_3;
            break;
            
        case CLOCK_PROFILE_400M:
            // 400MHz using PLL (25MHz HSE / 5 * 80 / 1 = 400MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE0;
            pll_m = 5;
            pll_n = 80;
            pll_p = 1;
            flash_latency = FLASH_LATENCY_4;
            break;
            
        case CLOCK_PROFILE_550M:
        default:
            // 550MHz using PLL (25MHz HSE / 5 * 110 / 1 = 550MHz)
            pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE0;
            pll_m = 5;
            pll_n = 110;
            pll_p = 1;
            flash_latency = FLASH_LATENCY_4;
            break;
    }

    SHELL_LOG_CLK_INFO("Starting clock switch to %lu Hz...", target_freq);

    /* Use global interrupt disable for maximum safety during clock switching. */
    __disable_irq();

    // --- Step 1: 升频时先配置电压调节器和Flash等待周期 ---
    if (profile >= CLOCK_PROFILE_400M)
    {
        // 高频模式需要最高电压等级
        __HAL_PWR_VOLTAGESCALING_CONFIG(pwr_vos_level);
        while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
        DelayNoCycles(1000000); // 约10ms延时
        __HAL_FLASH_SET_LATENCY(flash_latency);
    }
    else if (profile >= CLOCK_PROFILE_200M)
    {
        // 中高频模式
        __HAL_PWR_VOLTAGESCALING_CONFIG(pwr_vos_level);
        while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
        DelayNoCycles(500000); // 约5ms延时
    }

    // --- Step 2: 根据目标时钟源进行配置 ---
    if (use_pll == 0)
    {
        // 不使用PLL的低频模式
        if (profile == CLOCK_PROFILE_32K)
        {
            // 启用LSI
            RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
            RCC_OscInitStruct.LSIState = RCC_LSI_ON;
            RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
            if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
            {
                __enable_irq();
                SHELL_LOG_CLK_ERROR("LSI configuration failed");
                return HAL_ERROR;
            }
            
            // 切换到LSI
            RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
            RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
            if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK)
            {
                __enable_irq();
                SHELL_LOG_CLK_ERROR("LSI clock switch failed");
                return HAL_ERROR;
            }
        }
    }
    else
    {
        // 使用PLL的模式
        // 先切换到HSI作为临时时钟源
        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
        {
            __enable_irq();
            SHELL_LOG_CLK_ERROR("HSI temporary switch failed");
            return HAL_ERROR;
        }

        // 配置HSE和PLL
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        RCC_OscInitStruct.HSEState = RCC_HSE_ON;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        RCC_OscInitStruct.PLL.PLLM = pll_m;
        RCC_OscInitStruct.PLL.PLLN = pll_n;
        RCC_OscInitStruct.PLL.PLLFRACN = 0;
        RCC_OscInitStruct.PLL.PLLP = pll_p;
        RCC_OscInitStruct.PLL.PLLR = 2;
        RCC_OscInitStruct.PLL.PLLQ = 4;
        RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
        RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;

        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        {
            __enable_irq();
            SHELL_LOG_CLK_ERROR("PLL configuration failed");
            return HAL_ERROR;
        }
        
        // 等待PLL锁定
        uint32_t timeout = 100000;
        while(!__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) && timeout > 0)
        {
            timeout--;
        }
        if (timeout == 0)
        {
            __enable_irq();
            SHELL_LOG_CLK_ERROR("PLL lock timeout");
            return HAL_ERROR;
        }
        
        DelayNoCycles(500000); // PLL锁定后延时

        // 切换到PLL时钟源并配置总线时钟分频器
        RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                       RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
        RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
        RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
        RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK)
        {
            __enable_irq();
            SHELL_LOG_CLK_ERROR("PLL clock switch failed");
            return HAL_ERROR;
        }
    }

    // --- Step 3: 降频时配置电压调节器 ---
    if (profile < CLOCK_PROFILE_200M)
    {
        __HAL_PWR_VOLTAGESCALING_CONFIG(pwr_vos_level);
        while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
        DelayNoCycles(500000);
    }

    /* Re-enable interrupts */
    __enable_irq();

    // --- Step 4: 更新系统时钟变量和重新初始化关键外设 ---
    SystemCoreClockUpdate();
    
    // 重新配置SysTick，确保始终保持1ms时基
    // 无论系统时钟如何变化，都要保证SysTick产生准确的1ms中断
    uint32_t new_sysclk = HAL_RCC_GetSysClockFreq();
    
    // 停止当前SysTick
    SysTick->CTRL = 0;
    
    // 重新计算SysTick重载值，确保1ms时基
    // SysTick使用系统时钟作为时钟源
    uint32_t reload_value = (new_sysclk / 1000) - 1; // 1ms = sysclk/1000
    
    // 配置SysTick
    SysTick->LOAD = reload_value;
    SysTick->VAL = 0; // 清除当前值
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |  // 使用处理器时钟
                    SysTick_CTRL_TICKINT_Msk |    // 使能中断
                    SysTick_CTRL_ENABLE_Msk;      // 使能SysTick
    
    // 确保HAL时基也正确更新
    HAL_InitTick(TICK_INT_PRIORITY);
    
    SHELL_LOG_CLK_INFO("SysTick reconfigured for %lu Hz system clock (1ms tick)", new_sysclk);
    
    // 添加延时确保SysTick稳定后再重新初始化UART
    DelayNoCycles(100000); // 约1ms延时
    
    // 重新初始化UART（因为时钟变化会影响波特率）
    if (HAL_UART_Init(&huart3) != HAL_OK) 
    { 
        SHELL_LOG_CLK_ERROR("UART reinit failed");
        return HAL_ERROR;
    }

    uint32_t actual_freq = HAL_RCC_GetSysClockFreq();
    SHELL_LOG_CLK_INFO("Clock switched successfully to %lu Hz", actual_freq);
    
    return HAL_OK;
}

/**
  * @brief  获取当前系统时钟频率
  * @retval 系统时钟频率 (Hz)
  */
uint32_t GetCurrentSystemClock(void)
{
    return HAL_RCC_GetSysClockFreq();
}

/**
  * @brief  检查当前时钟配置文件
  * @retval 当前的时钟配置文件
  */
ClockProfile_t GetCurrentClockProfile(void)
{
    uint32_t sysclk = HAL_RCC_GetSysClockFreq();
    
    if (sysclk >= 500000000) return CLOCK_PROFILE_550M;
    else if (sysclk >= 350000000) return CLOCK_PROFILE_400M;
    else if (sysclk >= 250000000) return CLOCK_PROFILE_300M;
    else if (sysclk >= 150000000) return CLOCK_PROFILE_200M;
    else if (sysclk >= 110000000) return CLOCK_PROFILE_128M;
    else if (sysclk >= 80000000) return CLOCK_PROFILE_96M;
    else if (sysclk >= 40000000) return CLOCK_PROFILE_48M;
    else if (sysclk >= 1000000) return CLOCK_PROFILE_24M;
    else return CLOCK_PROFILE_32K;
}

/**
  * @brief  Find a compatible clock profile when the requested one is not compatible
  * @param  requested_profile The originally requested profile
  * @retval Compatible profile (may be the same if already compatible)
  */
static ClockProfile_t FindCompatibleProfile(ClockProfile_t requested_profile)
{
    // Try profiles from highest to lowest frequency
    ClockProfile_t profiles[] = {
        CLOCK_PROFILE_550M, CLOCK_PROFILE_400M, CLOCK_PROFILE_300M,
        CLOCK_PROFILE_200M, CLOCK_PROFILE_128M, CLOCK_PROFILE_96M,
        CLOCK_PROFILE_48M, CLOCK_PROFILE_24M
    };
    
    for (int i = 0; i < 8; i++)
    {
        uint32_t test_freq = GetProfileFrequency(profiles[i]);
        if (CheckPeripheralCompatibility(test_freq) == 0)
        {
            return profiles[i];
        }
    }
    
    // If no compatible profile found, return the requested one
    return requested_profile;
}