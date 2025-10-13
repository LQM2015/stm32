/**
 ******************************************************************************
 * @file    bootloader.c
 * @author  Your Name
 * @brief   STM32H750 Bootloader Implementation
 * @details Initializes QSPI Flash and jumps to application
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "bootloader.h"
#include "quadspi.h"
#include "debug.h"

#ifdef BOOTLOADER

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define APP_VALID_STACK_MIN 0x20000000U  /* Minimum valid stack pointer */
#define APP_VALID_STACK_MAX 0x24080000U  /* Maximum valid stack pointer */
#define APP_VALID_RESET_MIN 0x90000000U  /* Minimum valid reset vector */
#define APP_VALID_RESET_MAX 0x92000000U  /* Maximum valid reset vector */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static pFunction JumpToApplication;
static uint32_t JumpAddress;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initialize bootloader and prepare for application jump
 * @retval None
 */
void Bootloader_Init(void)
{
    DEBUG_INFO("===========================================");
    DEBUG_INFO("  STM32H750 Bootloader v%s", BOOTLOADER_VERSION);
    DEBUG_INFO("===========================================");
    DEBUG_INFO("Bootloader running from internal Flash");
    DEBUG_INFO("Target application at 0x%08X", APP_ADDRESS);
}

/**
 * @brief  Verify application is valid and ready to run
 * @retval 1 if application is valid, 0 otherwise
 */
uint8_t Bootloader_VerifyApplication(void)
{
    uint32_t app_stack = APP_STACK_POINTER();
    uint32_t app_reset = APP_RESET_VECTOR();
    
    DEBUG_INFO("Verifying application...");
    DEBUG_INFO("  Stack Pointer: 0x%08X", app_stack);
    DEBUG_INFO("  Reset Vector:  0x%08X", app_reset);
    
    /* Check if stack pointer is valid */
    if (app_stack < APP_VALID_STACK_MIN || app_stack > APP_VALID_STACK_MAX) {
        DEBUG_ERROR("Invalid stack pointer: 0x%08X", app_stack);
        DEBUG_ERROR("Valid range: 0x%08X - 0x%08X", 
                    APP_VALID_STACK_MIN, APP_VALID_STACK_MAX);
        return 0;
    }
    
    /* Check if reset vector is valid */
    if (app_reset < APP_VALID_RESET_MIN || app_reset > APP_VALID_RESET_MAX) {
        DEBUG_ERROR("Invalid reset vector: 0x%08X", app_reset);
        DEBUG_ERROR("Valid range: 0x%08X - 0x%08X", 
                    APP_VALID_RESET_MIN, APP_VALID_RESET_MAX);
        return 0;
    }
    
    /* Check if reset vector is odd (Thumb mode) */
    if ((app_reset & 0x1) == 0) {
        DEBUG_WARN("Reset vector should be odd (Thumb mode)");
    }
    
    DEBUG_INFO("Application verification passed!");
    return 1;
}

/**
 * @brief  Deinitialize peripherals before jumping
 * @retval None
 */
void Bootloader_DeInit(void)
{
    DEBUG_INFO("Deinitializing bootloader peripherals...");
    
    /* Disable interrupts */
    __disable_irq();
    
    /* Disable SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* Clear all pending interrupts */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  /* Disable all interrupts */
        NVIC->ICPR[i] = 0xFFFFFFFF;  /* Clear all pending interrupts */
    }
    
    /* IMPORTANT: Do NOT disable cache here! 
     * The application code is running from external Flash (0x90000000)
     * which requires cache to be enabled for proper execution.
     * Cache will be cleaned and invalidated, then re-enabled by the application.
     */
    
    /* Clean D-Cache to ensure all data is written to memory */
    SCB_CleanDCache();
    
    /* Relocate vector table to external Flash */
    SCB->VTOR = APP_ADDRESS;
    
    /* Memory barrier to ensure all writes complete */
    __DSB();
    __ISB();
    
    DEBUG_INFO("Bootloader deinitialization complete");
}

/**
 * @brief  Jump to application code in external Flash
 * @note   This function does not return
 * @retval None
 */
void Bootloader_JumpToApplication(void)
{
    /* Get stack pointer and reset vector */
    uint32_t app_stack = APP_STACK_POINTER();
    uint32_t app_reset = APP_RESET_VECTOR();
    
    DEBUG_INFO("===========================================");
    DEBUG_INFO("  Jumping to Application");
    DEBUG_INFO("===========================================");
    DEBUG_INFO("MSP will be set to: 0x%08X", app_stack);
    DEBUG_INFO("PC will jump to:    0x%08X", app_reset & 0xFFFFFFFE);
    DEBUG_INFO("VTOR will be set to: 0x%08X", APP_ADDRESS);
    DEBUG_INFO("Goodbye from bootloader...");
    DEBUG_INFO("");
    
    /* Small delay to ensure UART transmission completes */
    HAL_Delay(100);
    
    /* Deinitialize peripherals and relocate vector table */
    Bootloader_DeInit();
    
    /* Set Main Stack Pointer */
    __set_MSP(app_stack);
    
    /* Set Control register (use MSP, privileged mode) */
    __set_CONTROL(0);
    
    /* Jump to application reset handler */
    JumpAddress = app_reset;
    JumpToApplication = (pFunction)JumpAddress;
    JumpToApplication();
    
    /* Should never reach here */
    while(1) {
        /* Infinite loop in case of jump failure */
    }
}

/**
 * @brief  Print bootloader information
 * @retval None
 */
void Bootloader_PrintInfo(void)
{
    DEBUG_INFO("===========================================");
    DEBUG_INFO("  System Information");
    DEBUG_INFO("===========================================");
    DEBUG_INFO("CPU:          STM32H750XBHx");
    DEBUG_INFO("Core:         Cortex-M7 @ %lu MHz", HAL_RCC_GetSysClockFreq() / 1000000);
    DEBUG_INFO("Flash (Int):  128 KB");
    DEBUG_INFO("Flash (Ext):  32 MB (W25Q256)");
    DEBUG_INFO("RAM:          1 MB");
    DEBUG_INFO("Bootloader:   v%s", BOOTLOADER_VERSION);
    DEBUG_INFO("Build Date:   %s %s", __DATE__, __TIME__);
    DEBUG_INFO("===========================================");
}

#endif /* BOOTLOADER */
