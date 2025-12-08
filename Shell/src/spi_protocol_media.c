/**
 * @file spi_protocol_media.c
 * @brief SPI Protocol for Photo and Video Implementation
 * @version 1.0
 * @date 2025-01-22
 */

#include "spi_protocol_media.h"
#include "spi_protocol_ota.h"
#include <string.h>

/* =================================================================== */
/* Private Variables                                                  */
/* =================================================================== */

static uint32_t g_media_sequence_counter = 2000;  // Media protocol sequence number

/* Handshake data */
static const uint32_t g_linux_handshake_data[2] = {
    COMMON_LINUX_HANDSHAKE, 
    COMMON_HANDSHAKE_DATA
};

/* =================================================================== */
/* Private Functions                                                  */
/* =================================================================== */

/**
 * @brief Generic media protocol execution
 */
static int execute_media_protocol(business_type_t business_type)
{
    ctl_fr_t send_frame, recv_frame;
    int ret;
    const char *business_name;
    uint32_t business_data;
    uint32_t param_cmd;
    uint32_t success_cmd;
    
    // Set parameters based on business type
    if (business_type == BUSINESS_TYPE_PHOTO) {
        business_name = "Photo";
        business_data = PHOTO_BUSINESS_DATA;
        param_cmd = PHOTO_PARAM_CMD;
        success_cmd = PHOTO_SUCCESS_CMD;
    } else if (business_type == BUSINESS_TYPE_VIDEO) {
        business_name = "Video";
        business_data = VIDEO_BUSINESS_DATA;
        param_cmd = VIDEO_PARAM_CMD;
        success_cmd = VIDEO_SUCCESS_CMD;
    } else if (business_type == BUSINESS_TYPE_WIFI) {
        business_name = "WiFi";
        business_data = WIFI_BUSINESS_DATA;
        param_cmd = WIFI_PARAM_CMD;
        success_cmd = WIFI_SUCCESS_CMD;
    }
    
    TRACE_INFO("%s Protocol: Starting execution...", business_name);
    
    
    /* ===== Step 3: Send business handshake [0xFE, 0x0A/0x03] ===== */
    uint32_t business_handshake_data[2] = {COMMON_LINUX_HANDSHAKE, business_data};
    
    ret = spi_protocol_build_frame(&send_frame, NORMAL_SPI_CMD, 
                                    business_handshake_data, 
                                    2 * sizeof(uint32_t), 
                                    ++g_media_sequence_counter);
    if (ret != 0) {
        TRACE_ERROR("%s Protocol: Failed to build business handshake frame", business_name);
        return -6;
    }
    
    ret = spi_protocol_send_frame(&send_frame);
    if (ret != 0) {
        TRACE_ERROR("%s Protocol: Failed to send business handshake", business_name);
        return -7;
    }
    
    TRACE_INFO("%s Protocol: Step 3 - Sent business handshake [0x%02X, 0x%02X], seq=%d", 
               business_name, COMMON_LINUX_HANDSHAKE, business_data, send_frame.seqctl);
    
    /* ===== Step 4: Receive parameters ===== */
    platform_delay_ms(100);  // Wait for parameter response
    
    ret = spi_protocol_receive_frame(&recv_frame);
    if (ret != 0) {
        TRACE_ERROR("%s Protocol: Failed to receive parameters", business_name);
        return -8;
    }
    
    if (!spi_protocol_validate_frame(&recv_frame, param_cmd, 5 * sizeof(uint32_t))) {
        TRACE_ERROR("%s Protocol: Invalid parameter frame", business_name);
        return -9;
    }
    
    // Parse and display parameters
    if (business_type == BUSINESS_TYPE_PHOTO) {
        photo_param_t *photo_params = (photo_param_t*)recv_frame.data;
        TRACE_INFO("%s Protocol: Step 4 - Received params [cmd=0x%02X, num=%d, delay=%d, res=%d, time=%d]", 
                   business_name, photo_params->cmd, photo_params->num, 
                   photo_params->delay, photo_params->res, photo_params->time);
    } else if (business_type == BUSINESS_TYPE_VIDEO) {
        video_param_t *video_params = (video_param_t*)recv_frame.data;
        TRACE_INFO("%s Protocol: Step 4 - Received params [cmd=0x%02X, duration=%d, delay=%d, res=%d, timestamp=%d]", 
                   business_name, video_params->cmd, video_params->duration, 
                   video_params->delay, video_params->res, video_params->timestamp);
    } else if (business_type == BUSINESS_TYPE_WIFI) {
        wifi_param_t *wifi_params = (wifi_param_t*)recv_frame.data;
        TRACE_INFO("%s Protocol: Step 4 - Received params [cmd=0x%02X, switch=%s (0x%02X)]", 
                   business_name, wifi_params->cmd, 
                   wifi_params->sw == 0x01 ? "ON" : "OFF", wifi_params->sw);
    }
    
    /* ===== Step 5: Send success confirm ===== */
    platform_delay_ms(100);
    
    uint32_t success_data[2] = {success_cmd, PHOTO_SUCCESS_DATA};  // Both use 0x01
    
    ret = spi_protocol_build_frame(&send_frame, NORMAL_SPI_CMD, 
                                    success_data, 
                                    2 * sizeof(uint32_t), 
                                    ++g_media_sequence_counter);
    if (ret != 0) {
        TRACE_ERROR("%s Protocol: Failed to build success frame", business_name);
        return -10;
    }
    
    ret = spi_protocol_send_frame(&send_frame);
    if (ret != 0) {
        TRACE_ERROR("%s Protocol: Failed to send success confirm", business_name);
        return -11;
    }
    
    TRACE_INFO("%s Protocol: Step 5 - Sent success confirm [0x%02X, 0x%02X], seq=%d", 
               business_name, success_cmd, PHOTO_SUCCESS_DATA, send_frame.seqctl);
    
    TRACE_INFO("%s Protocol: Execution completed successfully!", business_name);
    
    // Optional: receive final confirmation
    platform_delay_ms(100);
    ret = spi_protocol_receive_frame(&recv_frame);
    if (ret == 0) {
        TRACE_DEBUG("%s Protocol: Received final confirmation", business_name);
    }
    
    return 0;
}

/* =================================================================== */
/* Public Functions                                                   */
/* =================================================================== */

/**
 * @brief Execute photo capture protocol
 */
int spi_protocol_photo_execute(void)
{
    return execute_media_protocol(BUSINESS_TYPE_PHOTO);
}

/**
 * @brief Execute video recording protocol
 */
int spi_protocol_video_execute(void)
{
    return execute_media_protocol(BUSINESS_TYPE_VIDEO);
}

/**
 * @brief Execute WiFi switch protocol
 */
int spi_protocol_wifi_execute(void)
{
    return execute_media_protocol(BUSINESS_TYPE_WIFI);
}

/**
 * @brief Execute auto-detect media protocol
 */
int spi_protocol_media_auto_execute(void)
{
    ctl_fr_t send_frame, recv_frame;
    int ret;
    business_type_t detected_type;
    
    TRACE_INFO("Media Protocol: Starting auto-detection...");
    
    /* ===== Step 1: Send Linux handshake ===== */
    TRACE_INFO("Media Protocol: Step 1 - Send Linux handshake");
    ret = spi_protocol_build_frame(&send_frame, NORMAL_SPI_CMD, 
                                    g_linux_handshake_data, 
                                    2 * sizeof(uint32_t), 
                                    ++g_media_sequence_counter);
    if (ret != 0) {
        TRACE_ERROR("Media Protocol: Failed to build Linux handshake frame");
        return -1;
    }
    
    ret = spi_protocol_send_frame(&send_frame);
    if (ret != 0) {
        TRACE_ERROR("Media Protocol: Failed to send Linux handshake");
        return -2;
    }
    
    TRACE_INFO("Media Protocol: Sent Linux handshake, waiting for business type...");
    
    /* ===== Step 2: Receive business confirm and detect type ===== */
    platform_delay_ms(50);

    TRACE_INFO("Media Protocol: Step 2 - Waiting for business confirm...");
    ret = spi_protocol_receive_frame(&recv_frame);
    if (ret != 0) {
        TRACE_ERROR("Media Protocol: Failed to receive business confirm");
        return -3;
    }
    
    if (!spi_protocol_validate_frame(&recv_frame, COMMON_BUSINESS_CONFIRM, 2 * sizeof(uint32_t))) {
        TRACE_ERROR("Media Protocol: Invalid business confirm frame");
        return -4;
    }
    
    // Auto-detect business type
    uint32_t recv_data[2];
    memcpy(recv_data, recv_frame.data, sizeof(uint32_t) * 2);
    
    if (recv_data[1] == PHOTO_BUSINESS_ACK) {
        detected_type = BUSINESS_TYPE_PHOTO;
        TRACE_INFO("Media Protocol: Detected PHOTO business (ack=0x%02X)", recv_data[1]);
    } else if (recv_data[1] == VIDEO_BUSINESS_ACK) {
        detected_type = BUSINESS_TYPE_VIDEO;
        TRACE_INFO("Media Protocol: Detected VIDEO business (ack=0x%02X)", recv_data[1]);
    } else if (recv_data[1] == WIFI_BUSINESS_ACK) {
        detected_type = BUSINESS_TYPE_WIFI;
        TRACE_INFO("Media Protocol: Detected WIFI business (ack=0x%02X)", recv_data[1]);
    } else if (recv_data[1] == OTA_BUSINESS_ACK) {
        // OTA upgrade protocol - switch to OTA handler
        TRACE_INFO("Media Protocol: Detected OTA business (ack=0x%02X)", recv_data[1]);
        TRACE_INFO("Media Protocol: Switching to OTA upgrade protocol handler...");
        
        // Call OTA upgrade protocol to instruct device to enter OTA boot mode
        // Note: This continues from step 3 (already completed Linux handshake and OTA confirm)
        ret = spi_protocol_ota_upgrade_execute();
        if (ret == 0) {
            TRACE_INFO("Media Protocol: OTA upgrade protocol completed successfully");
            TRACE_INFO("Media Protocol: Remote device should now restart into OTA boot mode");
        } else {
            TRACE_ERROR("Media Protocol: OTA upgrade protocol failed, ret=%d", ret);
        }
        return ret;
    } else {
        TRACE_ERROR("Media Protocol: Unknown business type (ack=0x%02X)", recv_data[1]);
        return -6;
    }
    
    // Continue with detected protocol
    // Note: Need to decrement sequence counter as it was incremented in previous step
    g_media_sequence_counter--;
    
    return execute_media_protocol(detected_type);
}

/**
 * @brief Parse photo parameters from received frame
 */
int spi_protocol_photo_parse_params(const ctl_fr_t *frame, photo_param_t *params)
{
    if (!frame || !params) {
        return -1;
    }
    
    if (frame->length < sizeof(photo_param_t)) {
        TRACE_ERROR("Frame length too small for photo parameters");
        return -2;
    }
    
    memcpy(params, frame->data, sizeof(photo_param_t));
    
    if (params->cmd != PHOTO_PARAM_CMD) {
        TRACE_ERROR("Invalid photo parameter command: 0x%02X", params->cmd);
        return -3;
    }
    
    return 0;
}

/**
 * @brief Parse video parameters from received frame
 */
int spi_protocol_video_parse_params(const ctl_fr_t *frame, video_param_t *params)
{
    if (!frame || !params) {
        return -1;
    }
    
    if (frame->length < sizeof(video_param_t)) {
        TRACE_ERROR("Frame length too small for video parameters");
        return -2;
    }
    
    memcpy(params, frame->data, sizeof(video_param_t));
    
    if (params->cmd != VIDEO_PARAM_CMD) {
        TRACE_ERROR("Invalid video parameter command: 0x%02X", params->cmd);
        return -3;
    }
    
    return 0;
}
