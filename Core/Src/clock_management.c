#include "main.h"
#include "clock_management.h"

/* External HAL Handle defined in main.c */
extern UART_HandleTypeDef huart3;

/**
  * @brief  Switches the system clock between pre-defined profiles.
  * @note   This function uses global interrupt disable (__disable_irq) for maximum safety.
  * @param  profile The target clock profile.
  * @retval None
  */
void SwitchSystemClock(ClockProfile_t profile)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    uint32_t flash_latency;
    uint32_t pwr_vos_level;

    /* Use global interrupt disable for maximum safety during clock switching. */
    __disable_irq();

    // --- Determine target settings ---
    if (profile == CLOCK_PROFILE_HIGH_PERF)
    {
        pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE0;
        RCC_OscInitStruct.PLL.PLLN = 220; // for 550 MHz
        flash_latency = FLASH_LATENCY_3;
    }
    else // CLOCK_PROFILE_POWER_SAVE
    {
        pwr_vos_level = PWR_REGULATOR_VOLTAGE_SCALE1;
        RCC_OscInitStruct.PLL.PLLN = 80; // for 200 MHz
        flash_latency = FLASH_LATENCY_2;
    }

    // --- Prepare for switch ---
    // When going FASTER, prepare VOS BEFORE the switch.
    if (profile == CLOCK_PROFILE_HIGH_PERF)
    {
        if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) { Error_Handler(); }
        __HAL_PWR_VOLTAGESCALING_CONFIG(pwr_vos_level);
        while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    }

    // --- The Switch ---
    // Step 1: Switch to HSI as a temporary, safe clock source.
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }

    // Step 2: Configure and enable the new PLL.
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    // Step 3: Switch system clock to the newly configured PLL and set all bus clocks.
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK) { Error_Handler(); }

    // --- Post-switch cleanup ---
    // When going SLOWER, set VOS AFTER the switch.
    if (profile == CLOCK_PROFILE_POWER_SAVE)
    {
        if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) { Error_Handler(); }
        __HAL_PWR_VOLTAGESCALING_CONFIG(pwr_vos_level);
        while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    }

    // Step 4: Update RTOS tick and other components
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);

    if (HAL_UART_Init(&huart3) != HAL_OK) { Error_Handler(); }

    /* Re-enable interrupts */
    __enable_irq();
}
