/**
 * @file stm32_aw882xx_adapter.c
 * @brief STM32 HAL adapter implementation for AW882XX SmartPA driver
 * @version 1.0.0
 * @date 2025-01-16
 * 
 * This file provides the platform-specific implementation for AW882XX SmartPA
 * driver on STM32 platform, including I2C communication, GPIO control, and
 * SAI/I2S audio interface.
 */

#include "stm32_aw882xx_adapter.h"
#include "main.h"
#include "i2c.h"
#include "sai.h"
#include "gpio.h"
#include "shell_log.h"
#include "aw882xx_init.h"
#include "aw882xx.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/********************************************
 * Include device-specific parameters
 * Modify the path according to your chip model
 *******************************************/
#include "config/AW88261/mono/32/aw_params.h"

/********************************************
 * Private defines
 *******************************************/
#define AW882XX_I2C_TIMEOUT     1000    /* I2C timeout in ms */
#define AW882XX_I2C_RETRY       3       /* I2C retry count */

/********************************************
 * External HAL handles (from CubeMX generated code)
 *******************************************/
extern I2C_HandleTypeDef hi2c1;
extern SAI_HandleTypeDef hsai_BlockA1;

/********************************************
 * Private variables
 *******************************************/
static bool g_aw882xx_initialized = false;
static bool g_sai_started = false;

/********************************************
 * Forward declarations
 *******************************************/
static int aw_dev0_i2c_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len);
static int aw_dev0_i2c_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len);
static void aw_dev0_reset_gpio_ctl(bool state);

#if (AW882XX_PA_NUM == 2)
static int aw_dev1_i2c_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len);
static int aw_dev1_i2c_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len);
static void aw_dev1_reset_gpio_ctl(bool state);
#endif

/********************************************
 * Chip initialization function declaration
 * Modify according to your chip model
 *******************************************/
extern int aw882xx_pid_2113_dev_init(void *aw882xx_val);

/********************************************
 * Device initialization info array
 *******************************************/
static struct aw_init_info g_aw_init_info[] = {
    {
        .dev_index = AW_DEV_0,
        .i2c_addr = AW882XX_DEV0_I2C_ADDR,
        .phase_sync = AW_PHASE_SYNC_DISABLE,
        .fade_en = AW_FADE_DISABLE,
        .mix_chip_count = AW_DEV0_MIX_CHIP_NUM,
        .prof_info = g_dev0_prof_info,
        .dev_init_ops = {aw882xx_pid_2113_dev_init},
        .i2c_read_func = aw_dev0_i2c_read,
        .i2c_write_func = aw_dev0_i2c_write,
        .reset_gpio_ctl = aw_dev0_reset_gpio_ctl,
    },
#if (AW882XX_PA_NUM == 2)
    {
        .dev_index = AW_DEV_1,
        .i2c_addr = AW882XX_DEV1_I2C_ADDR,
        .phase_sync = AW_PHASE_SYNC_DISABLE,
        .fade_en = AW_FADE_DISABLE,
        .mix_chip_count = AW_DEV0_MIX_CHIP_NUM,  /* Use same config for dev1 or create separate */
        .prof_info = g_dev0_prof_info,           /* Use same config for dev1 or create separate */
        .dev_init_ops = {aw882xx_pid_2113_dev_init},
        .i2c_read_func = aw_dev1_i2c_read,
        .i2c_write_func = aw_dev1_i2c_write,
        .reset_gpio_ctl = aw_dev1_reset_gpio_ctl,
    },
#endif
};

#define AW_INIT_INFO_COUNT  (sizeof(g_aw_init_info) / sizeof(g_aw_init_info[0]))

/********************************************
 * I2C Communication Implementation
 *******************************************/

/**
 * @brief I2C read function for device 0
 */
static int aw_dev0_i2c_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len)
{
    HAL_StatusTypeDef status;
    
    for (int retry = 0; retry < AW882XX_I2C_RETRY; retry++) {
        status = HAL_I2C_Mem_Read(&hi2c1, (dev_addr << 1), reg_addr,
                                  I2C_MEMADD_SIZE_8BIT, pdata, len, AW882XX_I2C_TIMEOUT);
        if (status == HAL_OK) {
            return 0;
        }
        AW_MS_DELAY(1);
    }
    
    SHELL_LOG_AW882XX_ERROR("I2C read failed: addr=0x%02X reg=0x%02X status=%d", 
                            dev_addr, reg_addr, status);
    return -1;
}

/**
 * @brief I2C write function for device 0
 */
static int aw_dev0_i2c_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len)
{
    HAL_StatusTypeDef status;
    
    for (int retry = 0; retry < AW882XX_I2C_RETRY; retry++) {
        status = HAL_I2C_Mem_Write(&hi2c1, (dev_addr << 1), reg_addr,
                                   I2C_MEMADD_SIZE_8BIT, pdata, len, AW882XX_I2C_TIMEOUT);
        if (status == HAL_OK) {
            return 0;
        }
        AW_MS_DELAY(1);
    }
    
    SHELL_LOG_AW882XX_ERROR("I2C write failed: addr=0x%02X reg=0x%02X status=%d", 
                            dev_addr, reg_addr, status);
    return -1;
}

#if (AW882XX_PA_NUM == 2)
/**
 * @brief I2C read function for device 1
 */
static int aw_dev1_i2c_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len)
{
    /* Same I2C bus as device 0 */
    return aw_dev0_i2c_read(dev_addr, reg_addr, pdata, len);
}

/**
 * @brief I2C write function for device 1
 */
static int aw_dev1_i2c_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *pdata, uint16_t len)
{
    /* Same I2C bus as device 0 */
    return aw_dev0_i2c_write(dev_addr, reg_addr, pdata, len);
}
#endif

/********************************************
 * GPIO Reset Control Implementation
 *******************************************/

/**
 * @brief Initialize reset GPIO pins
 */
static void aw882xx_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO clock - already done in MX_GPIO_Init() */
    
    /* Configure reset pin for device 0 */
    GPIO_InitStruct.Pin = AW882XX_DEV0_RST_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(AW882XX_DEV0_RST_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(AW882XX_DEV0_RST_GPIO_PORT, AW882XX_DEV0_RST_GPIO_PIN, GPIO_PIN_SET);
    
#if (AW882XX_PA_NUM == 2)
    /* Configure reset pin for device 1 */
    GPIO_InitStruct.Pin = AW882XX_DEV1_RST_GPIO_PIN;
    HAL_GPIO_Init(AW882XX_DEV1_RST_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(AW882XX_DEV1_RST_GPIO_PORT, AW882XX_DEV1_RST_GPIO_PIN, GPIO_PIN_SET);
#endif
    
    SHELL_LOG_AW882XX_DEBUG("GPIO initialized");
}

/**
 * @brief Reset GPIO control for device 0
 */
static void aw_dev0_reset_gpio_ctl(bool state)
{
    HAL_GPIO_WritePin(AW882XX_DEV0_RST_GPIO_PORT, AW882XX_DEV0_RST_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

#if (AW882XX_PA_NUM == 2)
/**
 * @brief Reset GPIO control for device 1
 */
static void aw_dev1_reset_gpio_ctl(bool state)
{
    HAL_GPIO_WritePin(AW882XX_DEV1_RST_GPIO_PORT, AW882XX_DEV1_RST_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
#endif

/**
 * @brief Execute reset sequence for all PA devices
 */
static void aw882xx_reset_sequence(void)
{
    /* Pull reset low */
    aw_dev0_reset_gpio_ctl(false);
#if (AW882XX_PA_NUM == 2)
    aw_dev1_reset_gpio_ctl(false);
#endif
    
    AW_MS_DELAY(1);
    
    /* Pull reset high */
    aw_dev0_reset_gpio_ctl(true);
#if (AW882XX_PA_NUM == 2)
    aw_dev1_reset_gpio_ctl(true);
#endif
    
    AW_MS_DELAY(2);
    
    SHELL_LOG_AW882XX_DEBUG("Reset sequence completed");
}

/********************************************
 * SAI/I2S Audio Interface Implementation
 *******************************************/

/**
 * @brief Configure SAI for I2S mode
 */
static int aw882xx_sai_configure(AW882XX_AudioConfig_t *cfg)
{
    /* SAI is already configured by CubeMX in MX_SAI1_Init() */
    /* If dynamic reconfiguration is needed, implement here */
    
    SHELL_LOG_AW882XX_INFO("SAI configured: rate=%lu, bits=%lu, ch=%lu",
                           cfg->sample_rate, cfg->data_size, cfg->channel_num);
    return 0;
}

/********************************************
 * Public API Implementation
 *******************************************/

int aw882xx_adapter_init(void)
{
    if (g_aw882xx_initialized) {
        SHELL_LOG_AW882XX_DEBUG("Already initialized");
        return 0;
    }
    
    SHELL_LOG_AW882XX_INFO("Initializing AW882XX adapter...");
    
    /* Initialize GPIO for reset pins */
    aw882xx_gpio_init();
    
    /* Execute reset sequence */
    aw882xx_reset_sequence();
    
    /* Initialize each PA device */
    size_t initialized_count = 0;
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        int ret = aw882xx_smartpa_init((void *)&g_aw_init_info[i]);
        if (ret != 0) {
            SHELL_LOG_AW882XX_ERROR("SmartPA init failed: dev=%d ret=%d", 
                                    g_aw_init_info[i].dev_index, ret);
            /* Cleanup already initialized devices */
            while (initialized_count > 0) {
                initialized_count--;
                aw882xx_smartpa_deinit(g_aw_init_info[initialized_count].dev_index);
            }
            return -1;
        }
        initialized_count++;
        SHELL_LOG_AW882XX_INFO("SmartPA dev%d initialized", g_aw_init_info[i].dev_index);
    }
    
    g_aw882xx_initialized = true;
    SHELL_LOG_AW882XX_INFO("AW882XX adapter initialized successfully");
    return 0;
}

void aw882xx_adapter_deinit(void)
{
    if (!g_aw882xx_initialized) {
        return;
    }
    
    SHELL_LOG_AW882XX_INFO("Deinitializing AW882XX adapter...");
    
    /* Stop if running */
    aw882xx_adapter_stop();
    
    /* Deinitialize each PA device */
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        aw882xx_smartpa_deinit(g_aw_init_info[i].dev_index);
    }
    
    /* Pull reset low to save power */
    aw_dev0_reset_gpio_ctl(false);
#if (AW882XX_PA_NUM == 2)
    aw_dev1_reset_gpio_ctl(false);
#endif
    
    g_aw882xx_initialized = false;
    SHELL_LOG_AW882XX_INFO("AW882XX adapter deinitialized");
}

int aw882xx_adapter_start(void)
{
    if (!g_aw882xx_initialized) {
        int ret = aw882xx_adapter_init();
        if (ret != 0) {
            return ret;
        }
    }
    
    SHELL_LOG_AW882XX_INFO("Starting AW882XX...");
    
    /* Set profile and start each device */
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        aw_dev_index_t dev = g_aw_init_info[i].dev_index;
        
        /* Set default profile */
        aw882xx_set_profile_byname(dev, "Music");
        
        /* Start the device */
        int ret = aw882xx_ctrl_state(dev, AW_START);
        if (ret != 0) {
            SHELL_LOG_AW882XX_ERROR("Failed to start dev%d: ret=%d", dev, ret);
            return -1;
        }
    }
    
    SHELL_LOG_AW882XX_INFO("AW882XX started");
    return 0;
}

int aw882xx_adapter_stop(void)
{
    if (!g_aw882xx_initialized) {
        return 0;
    }
    
    SHELL_LOG_AW882XX_INFO("Stopping AW882XX...");
    
    /* Stop each device */
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        aw_dev_index_t dev = g_aw_init_info[i].dev_index;
        aw882xx_ctrl_state(dev, AW_STOP);
    }
    
    SHELL_LOG_AW882XX_INFO("AW882XX stopped");
    return 0;
}

int aw882xx_stream_open(AW882XX_StreamType_t stream, AW882XX_Mode_t mode)
{
    SHELL_LOG_AW882XX_INFO("Opening stream: type=%d mode=%d", stream, mode);
    
    /* Initialize SmartPA adapter */
    int ret = aw882xx_adapter_init();
    if (ret != 0) {
        return ret;
    }
    
    /* SAI is already initialized by CubeMX */
    /* If dynamic mode switching is needed, reinitialize SAI here */
    
    return 0;
}

int aw882xx_stream_setup(AW882XX_StreamType_t stream, AW882XX_AudioConfig_t *cfg)
{
    if (cfg == NULL) {
        return -1;
    }
    
    SHELL_LOG_AW882XX_INFO("Setting up stream: rate=%lu bits=%lu ch=%lu",
                           cfg->sample_rate, cfg->data_size, cfg->channel_num);
    
    return aw882xx_sai_configure(cfg);
}

int aw882xx_stream_start(AW882XX_StreamType_t stream, AW882XX_Mode_t mode)
{
    SHELL_LOG_AW882XX_INFO("Starting stream: type=%d mode=%d", stream, mode);
    
    if (!g_sai_started) {
        /* Start SAI (enable clocks) */
        /* Note: Actual audio data transmission requires HAL_SAI_Transmit_DMA */
        g_sai_started = true;
    }
    
    /* Wait for I2S clocks to stabilize */
    AW_MS_DELAY(50);
    
    /* Start SmartPA */
    return aw882xx_adapter_start();
}

int aw882xx_stream_stop(AW882XX_StreamType_t stream, AW882XX_Mode_t mode)
{
    SHELL_LOG_AW882XX_INFO("Stopping stream: type=%d mode=%d", stream, mode);
    
    /* Stop SmartPA first */
    aw882xx_adapter_stop();
    
    if (g_sai_started) {
        /* Stop SAI DMA */
        HAL_SAI_DMAStop(&hsai_BlockA1);
        g_sai_started = false;
    }
    
    return 0;
}

int aw882xx_stream_close(AW882XX_StreamType_t stream, AW882XX_Mode_t mode)
{
    SHELL_LOG_AW882XX_INFO("Closing stream: type=%d mode=%d", stream, mode);
    
    /* Stop stream if running */
    aw882xx_stream_stop(stream, mode);
    
    /* Deinitialize adapter */
    aw882xx_adapter_deinit();
    
    return 0;
}

int aw882xx_audio_transmit_dma(uint8_t *pData, uint16_t Size)
{
    if (pData == NULL || Size == 0) {
        return -1;
    }
    
    HAL_StatusTypeDef status = HAL_SAI_Transmit_DMA(&hsai_BlockA1, pData, Size);
    if (status != HAL_OK) {
        SHELL_LOG_AW882XX_ERROR("SAI DMA transmit failed: status=%d", status);
        return -1;
    }
    
    return 0;
}

int aw882xx_set_profile(const char *profile_name)
{
    if (profile_name == NULL) {
        return -1;
    }
    
    if (!g_aw882xx_initialized) {
        SHELL_LOG_AW882XX_ERROR("Not initialized");
        return -1;
    }
    
    SHELL_LOG_AW882XX_INFO("Setting profile: %s", profile_name);
    
    /* Set profile for all devices */
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        aw_dev_index_t dev = g_aw_init_info[i].dev_index;
        int ret = aw882xx_set_profile_byname(dev, (char *)profile_name);
        if (ret != 0) {
            SHELL_LOG_AW882XX_ERROR("Failed to set profile for dev%d", dev);
            return -1;
        }
    }
    
    return 0;
}

int aw882xx_adapter_get_volume(uint32_t *volume)
{
    if (volume == NULL) {
        return -1;
    }
    
    if (!g_aw882xx_initialized) {
        return -1;
    }
    
    /* Get volume from first device */
    return aw882xx_get_volume(AW_DEV_0, volume);
}

int aw882xx_adapter_set_volume(uint32_t volume)
{
    if (!g_aw882xx_initialized) {
        return -1;
    }
    
    SHELL_LOG_AW882XX_INFO("Setting volume: %lu", volume);
    
    /* Set volume for all devices */
    for (size_t i = 0; i < AW_INIT_INFO_COUNT; i++) {
        aw_dev_index_t dev = g_aw_init_info[i].dev_index;
        int ret = aw882xx_set_volume(dev, volume);
        if (ret != 0) {
            return -1;
        }
    }
    
    return 0;
}

/********************************************
 * SAI DMA Callbacks (optional)
 *******************************************/

/**
 * @brief SAI TX complete callback
 */
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) {
        /* Handle TX complete - implement circular buffer logic here if needed */
    }
}

/**
 * @brief SAI TX half complete callback
 */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) {
        /* Handle TX half complete - implement double buffer logic here if needed */
    }
}

/**
 * @brief SAI error callback
 */
void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) {
        SHELL_LOG_AW882XX_ERROR("SAI error: code=0x%lX", hsai->ErrorCode);
    }
}
