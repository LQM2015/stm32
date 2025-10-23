/**
 * @file spi_protocol_ota.h
 * @brief SPI Protocol for OTA (Over-The-Air) Firmware Update
 * @version 1.0
 * @date 2025-01-22
 * 
 * @copyright Adapted from BES platform
 */

#ifndef __SPI_PROTOCOL_OTA_H__
#define __SPI_PROTOCOL_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "spi_protocol_common.h"

/* =================================================================== */
/* OTA Protocol Definitions                                           */
/* =================================================================== */

#define SPICOMM_LINKLAYER_DATA_SIZE  (1024 + 4)
#define LWK_OTA_BIN_MAX              3
#define LWK_OTA_BIN_NAME_LEN         128

/* OTA Package File Path - adjust according to your file system */
#define OTA_PACKAGE_PATH             "0:/data/ota_package.bin"

/* OTA Protocol Commands */
#define OTA_EMPTY_CMD                0x00  /*!< Empty frame command */
#define OTA_EMPTY_DATA               0x00  /*!< Empty frame data */
#define OTA_REQUEST_CMD              0x09  /*!< OTA request command */
#define OTA_RESPONSE_CMD             0x0A  /*!< OTA response command */
#define OTA_REQUEST_PACKAGE          0x02  /*!< Request package sub-command */
#define OTA_REQUEST_END              0x03  /*!< End upgrade sub-command */
#define OTA_RESPONSE_FAIL            0x00  /*!< Request failed */
#define OTA_RESPONSE_SUCCESS         0x01  /*!< Request success */
#define OTA_RESPONSE_NOT_EXIST       0x02  /*!< OTA package not exist */
#define OTA_RESPONSE_RESTART         0x03  /*!< Restart to OTA boot */

/* GPIO Event Flags for OTA Protocol */
#define GPIO_SPI_TRIGGER_EVENT       (1 << 0)  /*!< SPI trigger event */
#define GPIO_SPI_STOP_EVENT          (1 << 1)  /*!< SPI stop event */
#define GPIO_SPI_UBOOT_DET_EVENT     (1 << 2)  /*!< OTA boot detect event */
#define SPI_EVENT_TIMEOUT            (1 << 3)  /*!< SPI timeout event */
#define SPI_EVENT_TX_COMPLETE        (1 << 4)  /*!< SPI TX complete event */
#define SPI_EVENT_RX_COMPLETE        (1 << 5)  /*!< SPI RX complete event */

/* =================================================================== */
/* OTA File Information Structures                                    */
/* =================================================================== */

/**
 * @brief OTA user/file type enumeration
 */
typedef enum {
    BES_OTA_USER_INVALID = 0,
    BES_OTA_USER_FIRMWARE,           /*!< rtos_main.bin */
    BES_OTA_USER_LANGUAGE_PACKAGE,
    BES_OTA_USER_COMBOFIRMWARE,
    BES_OTA_USER_BTH = 7,            /*!< best1600_watch_bth.bin */
    BES_OTA_USER_FS,
    BES_OTA_USER_BTH_BOOTUP_INFO,
    BES_OTA_USER_BOOTUP_INFO,
    BES_OTA_USER_UPGRADE_LOG,
    BES_OTA_USER_NUM,
} bes_ota_user_t;

/**
 * @brief OTA file information structure
 */
typedef struct {
    uint32_t file_numbers;                          /*!< Number of files in package */
    int8_t file_name[LWK_OTA_BIN_MAX][LWK_OTA_BIN_NAME_LEN];  /*!< File names */
    uint32_t file_type[LWK_OTA_BIN_MAX];            /*!< File types */
    uint32_t file_start_addr[LWK_OTA_BIN_MAX];      /*!< File start addresses */
    uint32_t file_length[LWK_OTA_BIN_MAX];          /*!< File lengths */
    uint32_t crc32;                                 /*!< CRC32 checksum */
} __attribute__((packed)) OTA_FILE_INFO_T;

/**
 * @brief OTA file data structure
 */
typedef struct {
    int8_t file_data[SPICOMM_LINKLAYER_DATA_SIZE - 4];  /*!< File data */
    uint32_t crc32;                                      /*!< CRC32 checksum */
} __attribute__((packed)) OTA_FILE_DATA_T;

/**
 * @brief OTA request structure
 */
typedef struct {
    uint32_t cmd;         /*!< Command: 0x09 */
    uint32_t subcmd;      /*!< Sub-command: 0x02 (request) or 0x03 (end) */
    uint32_t ota_size;    /*!< OTA package size */
    char ota_name[32];    /*!< OTA package name */
} __attribute__((packed)) ota_request_t;

/**
 * @brief OTA response structure
 */
typedef struct {
    uint32_t cmd;         /*!< Command: 0x0A */
    uint32_t status;      /*!< Status: 0x00 (fail) or 0x01 (success) */
    uint32_t subcmd;      /*!< Sub-command: 0x02 (package) or 0x03 (restart) */
    uint32_t result;      /*!< Result code */
} __attribute__((packed)) ota_response_t;

/* =================================================================== */
/* OTA Transfer Mode                                                  */
/* =================================================================== */

/**
 * @brief OTA transfer mode enumeration
 */
typedef enum {
    OTA_TRANSFER_MODE_BLOCKING = 0,      /*!< Blocking mode with delays */
    OTA_TRANSFER_MODE_STATE_MACHINE = 1  /*!< State machine mode with timers */
} ota_transfer_mode_t;

/**
 * @brief OTA protocol state enumeration (for state machine mode)
 */
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_UPGRADE_LOCK_SENT,
    OTA_STATE_EMPTY_RESPONSE_WAIT,
    OTA_STATE_PACKAGE_LOCK_SENT,
    OTA_STATE_PACKAGE_REQUEST_WAIT,
    OTA_STATE_FILE_INFO_SENDING,
    OTA_STATE_FILE_DATA_SENDING,
    OTA_STATE_TRANSFER_COMPLETE
} ota_protocol_state_t;

/* =================================================================== */
/* OTA Configuration Functions                                        */
/* =================================================================== */

/**
 * @brief Set OTA transfer mode
 * @param mode Transfer mode (blocking or state machine)
 */
void spi_protocol_ota_set_mode(ota_transfer_mode_t mode);

/**
 * @brief Get current OTA transfer mode
 * @return Current transfer mode
 */
ota_transfer_mode_t spi_protocol_ota_get_mode(void);

/* =================================================================== */
/* OTA File Operations                                                */
/* =================================================================== */

/**
 * @brief Load OTA file information from package
 * @param ota_info Pointer to store file information
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_load_file_info(OTA_FILE_INFO_T *ota_info);

/**
 * @brief Read OTA file data
 * @param filename File name to read
 * @param offset Offset within file
 * @param ota_data Pointer to store file data
 * @return Number of bytes read on success, negative on error
 */
int spi_protocol_ota_read_file_data(const char *filename, uint32_t offset, 
                                     OTA_FILE_DATA_T *ota_data);

/**
 * @brief Check if OTA package exists
 * @param ota_size Expected package size
 * @param ota_name Expected package name
 * @return true if exists and matches, false otherwise
 */
bool spi_protocol_ota_check_package_exist(uint32_t ota_size, const char *ota_name);

/* =================================================================== */
/* OTA Upgrade Protocol (Application to OTA Boot)                    */
/* =================================================================== */

/**
 * @brief Execute OTA upgrade protocol
 * 
 * This protocol instructs the remote device (running application firmware)
 * to restart into OTA boot mode.
 * 
 * Protocol flow:
 * 1. Send Linux handshake [0xFE, 0x01]
 * 2. Receive OTA business confirm [0xFD, 0x01]
 * 3-4. Loop: Send empty frames [0x00, 0x00] until receiving OTA request [0x09, ...]
 * 5. Send OTA response based on package existence
 * 6. Send restart command [0x0A, 0x01, 0x03, 0x01] to enter OTA boot
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_upgrade_execute(void);

/* =================================================================== */
/* OTA Firmware Transfer Protocol (OTA Boot State)                   */
/* =================================================================== */

/**
 * @brief Execute OTA firmware transfer protocol
 * 
 * This protocol transfers firmware packages to the remote device
 * already in OTA boot mode.
 * 
 * Protocol flow:
 * 1. Send upgrade lock [0x0A, 0x01, 0x03, 0x01]
 * 2. Receive empty response [0x00, 0x00]
 * 3. Send package lock [0x0A, 0x00, 0x00, 0x01]
 * 4. Receive package request [0x09, 0x01]
 * 5. Send file info via spibuf[1028]
 * 6. Receive file info confirmation
 * 7-8. Loop for each file:
 *      - Send file data chunks via spibuf[1028]
 *      - Receive confirmation for each chunk
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_firmware_transfer_execute(void);

/**
 * @brief Execute unified OTA transfer (auto-select mode)
 * 
 * Automatically uses blocking or state machine mode based on configuration
 * 
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_transfer_execute(void);

/* =================================================================== */
/* OTA State Machine (Non-blocking Mode)                             */
/* =================================================================== */

/**
 * @brief Initialize OTA state machine
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_state_machine_init(void);

/**
 * @brief Process OTA state machine
 * @return 0 on success, negative on error
 */
int spi_protocol_ota_state_machine_process(void);

/**
 * @brief Deinitialize OTA state machine
 */
void spi_protocol_ota_state_machine_deinit(void);

/**
 * @brief Get current OTA state
 * @return Current protocol state
 */
ota_protocol_state_t spi_protocol_ota_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_PROTOCOL_OTA_H__ */
