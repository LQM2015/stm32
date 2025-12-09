/**
 * @file stm32_aw882xx_adapter.h
 * @brief STM32 HAL adapter for AW882XX SmartPA driver
 * @version 1.0.0
 * @date 2025-01-16
 */

#ifndef __STM32_AW882XX_ADAPTER_H__
#define __STM32_AW882XX_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/********************************************
 * Configuration - Modify according to your hardware
 *******************************************/

/* Number of PA devices (1 for mono, 2 for stereo) */
#define AW882XX_PA_NUM                  1

/* I2C addresses for each PA device */
#define AW882XX_DEV0_I2C_ADDR           0x34
#define AW882XX_DEV1_I2C_ADDR           0x35

/* Reset GPIO configuration - Modify according to your schematic */
#define AW882XX_DEV0_RST_GPIO_PORT      GPIOA
#define AW882XX_DEV0_RST_GPIO_PIN       GPIO_PIN_0

#if (AW882XX_PA_NUM == 2)
#define AW882XX_DEV1_RST_GPIO_PORT      GPIOA
#define AW882XX_DEV1_RST_GPIO_PIN       GPIO_PIN_3
#endif

/********************************************
 * Audio stream types
 *******************************************/
typedef enum {
    AW882XX_STREAM_PLAYBACK = 0,
    AW882XX_STREAM_CAPTURE,
} AW882XX_StreamType_t;

/********************************************
 * Audio mode types
 *******************************************/
typedef enum {
    AW882XX_MODE_I2S = 0,
    AW882XX_MODE_TDM,
    AW882XX_MODE_QTY,
} AW882XX_Mode_t;

/********************************************
 * SAI/I2S configuration structure
 *******************************************/
typedef struct {
    AW882XX_Mode_t mode;
    uint32_t sample_rate;
    uint32_t data_size;         /* 16, 24, or 32 bits */
    uint32_t channel_num;       /* Number of channels */
    bool use_dma;               /* Whether to use DMA */
} AW882XX_AudioConfig_t;

/********************************************
 * Public API Functions
 *******************************************/

/**
 * @brief Initialize AW882XX adapter (I2C, GPIO, etc.)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_adapter_init(void);

/**
 * @brief Deinitialize AW882XX adapter
 */
void aw882xx_adapter_deinit(void);

/**
 * @brief Start AW882XX SmartPA
 * @return 0 on success, negative error code on failure
 */
int aw882xx_adapter_start(void);

/**
 * @brief Stop AW882XX SmartPA
 * @return 0 on success, negative error code on failure
 */
int aw882xx_adapter_stop(void);

/**
 * @brief Open audio stream
 * @param stream Stream type (playback/capture)
 * @param mode Audio mode (I2S/TDM)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_stream_open(AW882XX_StreamType_t stream, AW882XX_Mode_t mode);

/**
 * @brief Configure audio stream
 * @param stream Stream type (playback/capture)
 * @param cfg Audio configuration
 * @return 0 on success, negative error code on failure
 */
int aw882xx_stream_setup(AW882XX_StreamType_t stream, AW882XX_AudioConfig_t *cfg);

/**
 * @brief Start audio stream
 * @param stream Stream type (playback/capture)
 * @param mode Audio mode (I2S/TDM)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_stream_start(AW882XX_StreamType_t stream, AW882XX_Mode_t mode);

/**
 * @brief Stop audio stream
 * @param stream Stream type (playback/capture)
 * @param mode Audio mode (I2S/TDM)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_stream_stop(AW882XX_StreamType_t stream, AW882XX_Mode_t mode);

/**
 * @brief Close audio stream
 * @param stream Stream type (playback/capture)
 * @param mode Audio mode (I2S/TDM)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_stream_close(AW882XX_StreamType_t stream, AW882XX_Mode_t mode);

/**
 * @brief Transmit audio data using DMA
 * @param pData Pointer to data buffer
 * @param Size Data size in bytes
 * @return 0 on success, negative error code on failure
 */
int aw882xx_audio_transmit_dma(uint8_t *pData, uint16_t Size);

/**
 * @brief Set audio profile by name
 * @param profile_name Profile name ("Music", "Receiver", etc.)
 * @return 0 on success, negative error code on failure
 */
int aw882xx_set_profile(const char *profile_name);

/**
 * @brief Get current volume
 * @param volume Pointer to store volume value
 * @return 0 on success, negative error code on failure
 */
int aw882xx_adapter_get_volume(uint32_t *volume);

/**
 * @brief Set volume
 * @param volume Volume value
 * @return 0 on success, negative error code on failure
 */
int aw882xx_adapter_set_volume(uint32_t volume);

#ifdef __cplusplus
}
#endif

#endif /* __STM32_AW882XX_ADAPTER_H__ */
