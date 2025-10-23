/**
 * @file spi_protocol_common.h
 * @brief SPI Protocol Common Definitions and Functions
 * @version 1.0
 * @date 2025-01-22
 * 
 * @copyright Adapted from BES platform
 */

#ifndef __SPI_PROTOCOL_COMMON_H__
#define __SPI_PROTOCOL_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "shell_spictrl_port.h"

/* =================================================================== */
/* Protocol Frame Definitions                                         */
/* =================================================================== */

#define CTRL_MODE_DATA_LEN  12

/**
 * @brief Frame control structure
 */
typedef struct {
    uint8_t ver;           /*!< Protocol version */
    uint8_t type;          /*!< Frame type */
    uint8_t retry_quit;    /*!< Retry/quit flag */
    uint8_t reserved;      /*!< Reserved byte */
} __attribute__((packed)) fr_ctl_t;

/**
 * @brief Control frame structure
 */
typedef struct {
    fr_ctl_t fr_ctrl;                      /*!< Frame control */
    uint32_t seqctl;                        /*!< Sequence control */
    uint32_t length;                        /*!< Data length */
    uint32_t data[CTRL_MODE_DATA_LEN];      /*!< Data payload */
    uint32_t fcs;                           /*!< Frame check sequence */
} __attribute__((packed)) ctl_fr_t;

/* =================================================================== */
/* Protocol Command Definitions                                       */
/* =================================================================== */

/* Frame Types */
#define CMD_VERSION                 0x01
#define NORMAL_SPI_CMD              0x02
#define OTA_SPI_CMD                 0x03

/* SPI Command Types (for data field) */
#define SPI_CMD_NORMAL              0x00  /*!< Normal command */
#define SPI_CMD_OTA                 0x01  /*!< OTA command */

/* Common Protocol Commands */
#define COMMON_LINUX_HANDSHAKE      0xFE  /*!< Linux handshake command */
#define COMMON_HANDSHAKE_DATA       0x01  /*!< Handshake data */
#define COMMON_BUSINESS_CONFIRM     0xFD  /*!< Business confirm command */

/* Photo Protocol Commands */
#define PHOTO_BUSINESS_ACK          0x0A  /*!< Photo business ack */
#define PHOTO_BUSINESS_DATA         0x0A  /*!< Photo business data */
#define PHOTO_PARAM_CMD             0x31  /*!< Photo parameter command */
#define PHOTO_SUCCESS_CMD           0x32  /*!< Photo success command */
#define PHOTO_SUCCESS_DATA          0x01  /*!< Photo success data */

/* Video Protocol Commands */
#define VIDEO_BUSINESS_ACK          0x03  /*!< Video business ack */
#define VIDEO_BUSINESS_DATA         0x03  /*!< Video business data */
#define VIDEO_PARAM_CMD             0x13  /*!< Video parameter command */
#define VIDEO_SUCCESS_CMD           0x14  /*!< Video success command */
#define VIDEO_SUCCESS_DATA          0x01  /*!< Video success data */

/* OTA Protocol Commands */
#define OTA_BUSINESS_ACK            0x01  /*!< OTA business ack */
#define OTA_BUSINESS_DATA           0x01  /*!< OTA business data */
#define OTA_EMPTY_CMD               0x00  /*!< Empty command */
#define OTA_EMPTY_DATA              0x00  /*!< Empty data */
#define OTA_REQUEST_CMD             0x09  /*!< OTA request command */
#define OTA_RESPONSE_CMD            0x0A  /*!< OTA response command */
#define OTA_REQUEST_PACKAGE         0x02  /*!< Request package sub-command */
#define OTA_REQUEST_END             0x03  /*!< End upgrade sub-command */
#define OTA_RESPONSE_FAIL           0x00  /*!< Response: fail */
#define OTA_RESPONSE_SUCCESS        0x01  /*!< Response: success */
#define OTA_RESPONSE_NOT_EXIST      0x02  /*!< Response: not exist */
#define OTA_RESPONSE_RESTART        0x03  /*!< Response: restart to OTA boot */

/* =================================================================== */
/* Protocol State Definitions                                         */
/* =================================================================== */

/**
 * @brief Business type enumeration
 */
typedef enum {
    BUSINESS_TYPE_PHOTO = 0,    /*!< Photo business */
    BUSINESS_TYPE_VIDEO = 1,    /*!< Video business */
    BUSINESS_TYPE_OTA = 2       /*!< OTA business */
} business_type_t;

/**
 * @brief Protocol state enumeration
 */
typedef enum {
    PROTOCOL_STATE_IDLE = 0,
    PROTOCOL_STATE_LINUX_HANDSHAKE_SENT,
    PROTOCOL_STATE_BUSINESS_CONFIRM_RECEIVED,
    PROTOCOL_STATE_BUSINESS_HANDSHAKE_SENT,
    PROTOCOL_STATE_PARAM_RECEIVED,
    PROTOCOL_STATE_SUCCESS_SENT,
    PROTOCOL_STATE_COMPLETED
} protocol_state_t;

/* =================================================================== */
/* Frame Check Sequence (FCS) Functions                              */
/* =================================================================== */

/**
 * @brief Calculate FCS checksum for control frame
 * @param frame Pointer to control frame
 * @return Calculated FCS value
 */
uint32_t spi_protocol_calculate_fcs(const ctl_fr_t *frame);

/**
 * @brief Verify FCS checksum of control frame
 * @param frame Pointer to control frame
 * @return true if valid, false if invalid
 */
bool spi_protocol_verify_fcs(const ctl_fr_t *frame);

/* =================================================================== */
/* CRC32 Functions                                                    */
/* =================================================================== */

/**
 * @brief Calculate CRC32 checksum
 * @param data Pointer to data buffer
 * @param length Data length in bytes
 * @return CRC32 value
 */
uint32_t spi_protocol_calculate_crc32(const uint8_t *data, size_t length);

/* =================================================================== */
/* Frame Building and Communication                                   */
/* =================================================================== */

/**
 * @brief Build control frame
 * @param frame Pointer to frame buffer
 * @param type Frame type
 * @param data Pointer to data payload
 * @param data_len Data length in bytes
 * @param seq_num Sequence number
 * @return 0 on success, negative on error
 */
int spi_protocol_build_frame(ctl_fr_t *frame, uint8_t type, 
                              const uint32_t *data, size_t data_len, 
                              uint32_t seq_num);

/**
 * @brief Send control frame via SPI
 * @param frame Pointer to control frame
 * @return 0 on success, negative on error
 */
int spi_protocol_send_frame(const ctl_fr_t *frame);

/**
 * @brief Receive control frame via SPI
 * @param frame Pointer to frame buffer
 * @return 0 on success, negative on error
 */
int spi_protocol_receive_frame(ctl_fr_t *frame);

/**
 * @brief Validate received frame
 * @param frame Pointer to received frame
 * @param expected_cmd Expected command code
 * @param expected_min_len Expected minimum data length
 * @return true if valid, false if invalid
 */
bool spi_protocol_validate_frame(const ctl_fr_t *frame, 
                                  uint32_t expected_cmd, 
                                  size_t expected_min_len);

/* =================================================================== */
/* SPI Initialization                                                 */
/* =================================================================== */

/**
 * @brief Initialize SPI interface
 * @return 0 on success, negative on error
 */
int spi_protocol_init(void);

/**
 * @brief Deinitialize SPI interface
 * @return 0 on success, negative on error
 */
int spi_protocol_deinit(void);

/**
 * @brief Check if SPI is initialized
 * @return true if initialized, false otherwise
 */
bool spi_protocol_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_PROTOCOL_COMMON_H__ */
