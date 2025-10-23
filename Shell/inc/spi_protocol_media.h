/**
 * @file spi_protocol_media.h
 * @brief SPI Protocol for Photo and Video (Media Capture)
 * @version 1.0
 * @date 2025-01-22
 * 
 * @copyright Adapted from BES platform
 * 
 * @note Photo and video protocols share the same flow with different commands
 */

#ifndef __SPI_PROTOCOL_MEDIA_H__
#define __SPI_PROTOCOL_MEDIA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "spi_protocol_common.h"

/* =================================================================== */
/* Media Protocol Parameter Structures                                */
/* =================================================================== */

/**
 * @brief Photo parameters structure
 */
typedef struct {
    uint32_t cmd;        /*!< Command: 0x31 */
    uint32_t num;        /*!< Number of photos */
    uint32_t delay;      /*!< Delay time */
    uint32_t res;        /*!< Resolution */
    uint32_t time;       /*!< Timestamp */
} __attribute__((packed)) photo_param_t;

/**
 * @brief Video parameters structure
 */
typedef struct {
    uint32_t cmd;        /*!< Command: 0x13 */
    uint32_t duration;   /*!< Recording duration */
    uint32_t delay;      /*!< Delay time */
    uint32_t res;        /*!< Resolution */
    uint32_t timestamp;  /*!< Timestamp */
} __attribute__((packed)) video_param_t;

/* =================================================================== */
/* Media Protocol Functions                                           */
/* =================================================================== */

/**
 * @brief Execute photo capture protocol
 * 
 * Protocol flow:
 * 1. Send Linux handshake [0xFE, 0x01]
 * 2. Receive business confirm [0xFD, 0x0A]
 * 3. Send business handshake [0xFE, 0x0A]
 * 4. Receive photo parameters [0x31, num, delay, res, time]
 * 5. Send success confirm [0x32, 0x01]
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_photo_execute(void);

/**
 * @brief Execute video recording protocol
 * 
 * Protocol flow:
 * 1. Send Linux handshake [0xFE, 0x01]
 * 2. Receive business confirm [0xFD, 0x03]
 * 3. Send business handshake [0xFE, 0x03]
 * 4. Receive video parameters [0x13, duration, delay, res, timestamp]
 * 5. Send success confirm [0x14, 0x01]
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_video_execute(void);

/**
 * @brief Execute auto-detect media protocol
 * 
 * Automatically detects whether it's photo or video based on
 * the business confirm response (0x0A for photo, 0x03 for video)
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_media_auto_execute(void);

/**
 * @brief Parse photo parameters from received frame
 * @param frame Pointer to received frame
 * @param params Pointer to store parsed parameters
 * @return 0 on success, negative on error
 */
int spi_protocol_photo_parse_params(const ctl_fr_t *frame, photo_param_t *params);

/**
 * @brief Parse video parameters from received frame
 * @param frame Pointer to received frame
 * @param params Pointer to store parsed parameters
 * @return 0 on success, negative on error
 */
int spi_protocol_video_parse_params(const ctl_fr_t *frame, video_param_t *params);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_PROTOCOL_MEDIA_H__ */
