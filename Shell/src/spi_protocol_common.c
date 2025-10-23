/**
 * @file spi_protocol_common.c
 * @brief SPI Protocol Common Implementation
 * @version 1.0
 * @date 2025-01-22
 * 
 * @copyright Adapted from BES platform
 */

#include "spi_protocol_common.h"
#include <string.h>

/* =================================================================== */
/* Private Variables                                                  */
/* =================================================================== */

static bool g_spi_initialized = false;

/* =================================================================== */
/* FCS (Frame Check Sequence) Functions                              */
/* =================================================================== */

/**
 * @brief Calculate FCS checksum (simple addition checksum)
 */
uint32_t spi_protocol_calculate_fcs(const ctl_fr_t *frame)
{
    uint32_t fcs = 0;
    const uint8_t *data = (const uint8_t *)frame;
    size_t len = sizeof(ctl_fr_t) - sizeof(uint32_t); // Exclude FCS field itself
    
    for (size_t i = 0; i < len; i++) {
        fcs += data[i];
    }
    return fcs;
}

/**
 * @brief Verify FCS checksum
 */
bool spi_protocol_verify_fcs(const ctl_fr_t *frame)
{
    uint32_t calculated_fcs = spi_protocol_calculate_fcs(frame);
    return (calculated_fcs == frame->fcs);
}

/* =================================================================== */
/* CRC32 Functions                                                    */
/* =================================================================== */

/**
 * @brief Calculate CRC32 checksum (standard algorithm)
 */
uint32_t spi_protocol_calculate_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

/* =================================================================== */
/* Frame Building and Communication                                   */
/* =================================================================== */

/**
 * @brief Build control frame
 */
int spi_protocol_build_frame(ctl_fr_t *frame, uint8_t type, 
                              const uint32_t *data, size_t data_len, 
                              uint32_t seq_num)
{
    if (!frame) {
        return -1;
    }
    
    if (data_len > CTRL_MODE_DATA_LEN * sizeof(uint32_t)) {
        TRACE_ERROR("Frame data length exceeds limit: %d > %d", 
                    data_len, CTRL_MODE_DATA_LEN * sizeof(uint32_t));
        return -2;
    }
    
    // Initialize frame
    memset(frame, 0, sizeof(ctl_fr_t));
    
    // Fill frame control
    frame->fr_ctrl.ver = CMD_VERSION;
    frame->fr_ctrl.type = type;
    frame->fr_ctrl.retry_quit = 0x00;
    frame->fr_ctrl.reserved = 0x00;
    
    // Fill sequence and length
    frame->seqctl = seq_num;
    frame->length = data_len;
    
    // Copy data payload
    if (data && data_len > 0) {
        memcpy(frame->data, data, data_len);
    }
    
    // Calculate and set FCS
    frame->fcs = spi_protocol_calculate_fcs(frame);
    
    return 0;
}

/**
 * @brief Send control frame via SPI
 */
int spi_protocol_send_frame(const ctl_fr_t *frame)
{
    if (!frame) {
        return -1;
    }
    
    if (!g_spi_initialized) {
        TRACE_ERROR("SPI not initialized");
        return -2;
    }
    
    // Dump first 20 bytes of send data
    const uint8_t *send_data = (const uint8_t*)frame;
    size_t dump_len = (sizeof(ctl_fr_t) < 20) ? sizeof(ctl_fr_t) : 20;
    TRACE_INFO("SPI TX [%d bytes]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
               sizeof(ctl_fr_t),
               send_data[0], send_data[1], send_data[2], send_data[3], send_data[4],
               send_data[5], send_data[6], send_data[7], send_data[8], send_data[9],
               send_data[10], send_data[11], send_data[12], send_data[13], send_data[14],
               send_data[15], send_data[16], send_data[17], send_data[18], send_data[19]);
    
    int ret = platform_spi_transmit((const uint8_t*)frame, sizeof(ctl_fr_t));
    if (ret != 0) {
        TRACE_ERROR("SPI transmit failed: %d", ret);
        return -3;
    }
    
    return 0;
}

/**
 * @brief Receive control frame via SPI
 */
int spi_protocol_receive_frame(ctl_fr_t *frame)
{
    if (!frame) {
        return -1;
    }
    
    if (!g_spi_initialized) {
        TRACE_ERROR("SPI not initialized");
        return -2;
    }
    
    // Clear receive buffer
    memset(frame, 0x00, sizeof(ctl_fr_t));
    
    // Receive frame via SPI
    int ret = platform_spi_receive((uint8_t*)frame, sizeof(ctl_fr_t));
    if (ret != 0) {
        TRACE_ERROR("SPI receive failed: %d", ret);
        return -3;
    }
    
    // Dump first 20 bytes of received data
    const uint8_t *recv_data = (const uint8_t*)frame;
    TRACE_INFO("SPI RX [%d bytes]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
               sizeof(ctl_fr_t),
               recv_data[0], recv_data[1], recv_data[2], recv_data[3], recv_data[4],
               recv_data[5], recv_data[6], recv_data[7], recv_data[8], recv_data[9],
               recv_data[10], recv_data[11], recv_data[12], recv_data[13], recv_data[14],
               recv_data[15], recv_data[16], recv_data[17], recv_data[18], recv_data[19]);
    
    // Verify FCS
    if (!spi_protocol_verify_fcs(frame)) {
        TRACE_ERROR("Received frame FCS verification failed");
        return -4;
    }
    
    return 0;
}

/**
 * @brief Validate received frame
 */
bool spi_protocol_validate_frame(const ctl_fr_t *frame, 
                                  uint32_t expected_cmd, 
                                  size_t expected_min_len)
{
    if (!frame) {
        return false;
    }
    
    // Check version
    if (frame->fr_ctrl.ver != CMD_VERSION) {
        TRACE_ERROR("Invalid frame version: 0x%02X", frame->fr_ctrl.ver);
        return false;
    }
    
    // Check length
    if (frame->length < expected_min_len) {
        TRACE_ERROR("Invalid frame length: %d < %d", frame->length, expected_min_len);
        return false;
    }
    
    // Check command (if data exists)
    if (frame->length > 0) {
        uint32_t data0;
        memcpy(&data0, frame->data, sizeof(uint32_t));
        if (data0 != expected_cmd) {
            TRACE_ERROR("Invalid command: 0x%02X, expected 0x%02X", data0, expected_cmd);
            return false;
        }
    }
    
    return true;
}

/* =================================================================== */
/* SPI Initialization                                                 */
/* =================================================================== */

/**
 * @brief Initialize SPI interface
 */
int spi_protocol_init(void)
{
    if (g_spi_initialized) {
        TRACE_INFO("SPI already initialized");
        return 0;
    }
    
    int ret = platform_spi_init();
    if (ret != 0) {
        TRACE_ERROR("Failed to initialize SPI: %d", ret);
        return ret;
    }
    
    g_spi_initialized = true;
    TRACE_INFO("SPI initialized successfully");
    return 0;
}

/**
 * @brief Deinitialize SPI interface
 */
int spi_protocol_deinit(void)
{
    if (!g_spi_initialized) {
        return 0;
    }
    
    int ret = platform_spi_deinit();
    if (ret != 0) {
        TRACE_ERROR("Failed to deinitialize SPI: %d", ret);
        return ret;
    }
    
    g_spi_initialized = false;
    TRACE_INFO("SPI deinitialized");
    return 0;
}

/**
 * @brief Check if SPI is initialized
 */
bool spi_protocol_is_initialized(void)
{
    return g_spi_initialized;
}
