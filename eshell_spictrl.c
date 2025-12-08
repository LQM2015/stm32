/***************************************************************************
 *
 * Copyright 2015-2025 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/

#if !defined(DISABLE_ESHELL_SYSFREQ)
/***************************************************************************
 * Header Files
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "eshell.h"
#include "plat_types.h"
#include "hal_spi.h"
#include "hal_iomux.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hal_sysfreq.h"

/***************************************************************************
 * Private Types
 ****************************************************************************/
#define CTRL_MODE_DATA_LEN  12
#define SPICOMM_LINKLAYER_DATA_SIZE  1024+4
#define LWK_OTA_BIN_MAX  3
#define LWK_OTA_BIN_NAME_LEN 128

enum
{
    BES_OTA_USER_INVALID            = 0,
    BES_OTA_USER_FIRMWARE,               //rtos_main.bin
    BES_OTA_USER_LANGUAGE_PACKAGE,
    BES_OTA_USER_COMBOFIRMWARE,
    BES_OTA_USER_BTH = 7,              //best1600_watch_bth.bin
    BES_OTA_USER_FS,
    BES_OTA_USER_BTH_BOOTUP_INFO,
    BES_OTA_USER_BOOTUP_INFO,
    BES_OTA_USER_UPGRADE_LOG,

    BES_OTA_USER_NUM,
};

typedef struct {
    uint8_t ver;
    uint8_t type;
    uint8_t retry_quit;
    uint8_t reserved;
} __attribute__((packed)) fr_ctl_t;

// contrl frame
typedef struct {
    fr_ctl_t fr_ctrl;
    uint32_t seqctl;
    uint32_t length;
    uint32_t data[CTRL_MODE_DATA_LEN];
    uint32_t fcs;
} __attribute__((packed)) ctl_fr_t;

struct OTA_FILE_INFO_T {
    uint32_t   file_numbers;
    int8_t     file_name[LWK_OTA_BIN_MAX][LWK_OTA_BIN_NAME_LEN];
    uint32_t   file_type[LWK_OTA_BIN_MAX];
    uint32_t   file_start_addr[LWK_OTA_BIN_MAX];
    uint32_t   file_length[LWK_OTA_BIN_MAX];
    uint32_t   crc32;
};

struct OTA_FILE_DATA_T {
    int8_t     file_data[SPICOMM_LINKLAYER_DATA_SIZE-4];
    uint32_t   crc32;
};



// GPIO中断触发事件定义
#define GPIO_SPI_TRIGGER_EVENT    (1 << 0)
#define GPIO_SPI_STOP_EVENT       (1 << 1)
#define GPIO_SPI_UBOOT_DET_EVENT  (1 << 2)

// 添加状态机和事件定义
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

typedef enum {
    SPI_EVENT_TIMEOUT = (1 << 8),
    SPI_EVENT_TX_COMPLETE = (1 << 9),
    SPI_EVENT_RX_COMPLETE = (1 << 10),
    SPI_EVENT_ERROR = (1 << 11)
} spi_protocol_event_t;

// 状态机相关变量
static ota_protocol_state_t ota_current_state = OTA_STATE_IDLE;
static osTimerId_t ota_state_timer_id = NULL;
static uint32_t ota_retry_count = 0;
static uint32_t ota_file_index = 0;
static uint32_t ota_bytes_sent = 0;
static struct OTA_FILE_INFO_T ota_file_info; // 全局静态变量，供状态机函数使用

/***************************************************************************
 * Private Datas
 ****************************************************************************/
static const char spictrl_helper[] = \
        "Usage:" ESHELL_NEW_LINE \
        "  spictrl <help>" ESHELL_NEW_LINE \
        "    spictrl usage" ESHELL_NEW_LINE \
        "  spictrl <send> [hex_data]" ESHELL_NEW_LINE \
        "    send hex data in ctl_fr_t frame format" ESHELL_NEW_LINE \
        "  spictrl <recv>" ESHELL_NEW_LINE \
        "    receive ctl_fr_t control frame (fixed size)" ESHELL_NEW_LINE \
        "  spictrl <transfer> [send_hex_data]" ESHELL_NEW_LINE \
        "    send control frame and receive response frame" ESHELL_NEW_LINE \
        "  spictrl <gpio_auto> [gpio_pin] [enable]" ESHELL_NEW_LINE \
        "    enable/disable gpio interrupt auto spi mode (default GPIO06 for BES2800->BES2700 OTA)" ESHELL_NEW_LINE \
        "  spictrl <photo>" ESHELL_NEW_LINE \
        "    execute auto-detect business protocol (same as video)" ESHELL_NEW_LINE \
        "  spictrl <video>" ESHELL_NEW_LINE \
        "    execute auto-detect business protocol (same as photo)" ESHELL_NEW_LINE \
        "  spictrl <ota_start>" ESHELL_NEW_LINE \
        "    start OTA firmware transfer monitoring on GPIO06 (BES2800->BES2700)" ESHELL_NEW_LINE \
        "  spictrl <ota_stop>" ESHELL_NEW_LINE \
        "    stop OTA firmware transfer monitoring" ESHELL_NEW_LINE \
        "  spictrl <ota_test>" ESHELL_NEW_LINE \
        "    test OTA package file reading and parsing" ESHELL_NEW_LINE \
        "  spictrl <ota_mode> [mode]" ESHELL_NEW_LINE \
        "    get/set OTA transfer mode: 0=BLOCKING (osDelay), 1=STATE_MACHINE (timer)" ESHELL_NEW_LINE \
        "Note: All operations use ctl_fr_t structure with FCS validation" ESHELL_NEW_LINE \
        "Auto-detect: Business type is determined by received confirm (0x0A=Photo, 0x03=Video, 0x01=OTA)" ESHELL_NEW_LINE \
        "Photo Protocol: [0xFE,0x01] -> [0xFD,0x0A] -> [0xFE,0x0A] -> [0x31,num,delay,res,time] -> [0x32,0x01]" ESHELL_NEW_LINE \
        "Video Protocol: [0xFE,0x01] -> [0xFD,0x03] -> [0xFE,0x03] -> [0x13,duration,delay,res,timestamp] -> [0x14,0x01]" ESHELL_NEW_LINE \
        "OTA Protocol: GPIO06 detection -> [0x0A,0x01,0x03,0x01] -> [0x00,0x00] -> [0x0A,0x00,0x00,0x01] -> [0x09,0x01] -> spibuf[OTA_FILE_INFO_T] -> spibuf[OTA_FILE_DATA_T]" ESHELL_NEW_LINE \
        "OTA Transfer Modes: BLOCKING=synchronous with osDelay(), STATE_MACHINE=asynchronous with timers" ESHELL_NEW_LINE;

// GPIO自动SPI通信相关变量
static osThreadId_t gpio_spi_thread_id = NULL;
static osEventFlagsId_t gpio_spi_event_flags = NULL;
static int auto_gpio_pin = -1; // 无效引脚初始值
static bool gpio_auto_enabled = false;

// SPI自动接收配置
#define AUTO_SPI_HANDSHAKE_DATA_SIZE     2*sizeof(uint32_t)
#define AUTO_SPI_MAX_RECV_SIZE      256
#define AUTO_SPI_RECV_TIMEOUT_MS    500
#define GPIO_DEBOUNCE_DELAY_MS      500
#define OTA_SPI_CMD                 0x03
#define NORMAL_SPI_CMD              0x02
#define CMD_VERSION                 0x01

// 通用协议命令定义
#define COMMON_LINUX_HANDSHAKE     0xFE  // Linux握手命令
#define COMMON_HANDSHAKE_DATA       0x01  // 握手数据
#define COMMON_BUSINESS_CONFIRM     0xFD  // 业务确认命令

// 拍照协议命令定义
#define PHOTO_BUSINESS_ACK          0x0A  // 拍照业务确认数据
#define PHOTO_BUSINESS_DATA         0x0A  // 拍照业务握手数据
#define PHOTO_PARAM_CMD             0x31  // 拍照参数命令
#define PHOTO_SUCCESS_CMD           0x32  // 拍照成功命令
#define PHOTO_SUCCESS_DATA          0x01  // 拍照成功数据

// 录像协议命令定义
#define VIDEO_BUSINESS_ACK          0x03  // 录像业务确认数据
#define VIDEO_BUSINESS_DATA         0x03  // 录像业务握手数据
#define VIDEO_PARAM_CMD             0x13  // 录像参数命令
#define VIDEO_SUCCESS_CMD           0x14  // 录像成功命令
#define VIDEO_SUCCESS_DATA          0x01  // 录像成功数据

// OTA协议命令定义
#define OTA_BUSINESS_ACK            0x01  // OTA业务确认数据
#define OTA_BUSINESS_DATA           0x01  // OTA业务握手数据
#define OTA_EMPTY_CMD               0x00  // 空包命令
#define OTA_EMPTY_DATA              0x00  // 空包数据
#define OTA_REQUEST_CMD             0x09  // OTA请求命令
#define OTA_RESPONSE_CMD            0x0A  // OTA响应命令
#define OTA_REQUEST_PACKAGE         0x02  // 请求OTA包子命令
#define OTA_REQUEST_END             0x03  // 结束升级子命令
#define OTA_RESPONSE_FAIL           0x00  // 请求失败
#define OTA_RESPONSE_SUCCESS        0x01  // 请求成功
#define OTA_RESPONSE_NOT_EXIST      0x02  // OTA包不存在
#define OTA_RESPONSE_RESTART        0x03  // 重启进入OTA boot

// 业务类型定义
typedef enum {
    BUSINESS_TYPE_PHOTO = 0,
    BUSINESS_TYPE_VIDEO = 1,
    BUSINESS_TYPE_OTA = 2
} business_type_t;

// 协议状态定义
typedef enum {
    PROTOCOL_STATE_IDLE = 0,
    PROTOCOL_STATE_LINUX_HANDSHAKE_SENT,
    PROTOCOL_STATE_BUSINESS_CONFIRM_RECEIVED,
    PROTOCOL_STATE_BUSINESS_HANDSHAKE_SENT,
    PROTOCOL_STATE_PARAM_RECEIVED,
    PROTOCOL_STATE_SUCCESS_SENT,
    PROTOCOL_STATE_COMPLETED
} protocol_state_t;

// 拍照参数结构体
typedef struct {
    uint32_t cmd;        // 0x31
    uint32_t num;        // 拍照数量
    uint32_t delay;      // 延时
    uint32_t res;        // 分辨率
    uint32_t time;       // 时间
} __attribute__((packed)) photo_param_t;

// 录像参数结构体
typedef struct {
    uint32_t cmd;        // 0x13
    uint32_t duration;   // 录像时长
    uint32_t delay;      // 延时
    uint32_t res;        // 分辨率
    uint32_t timestamp;  // 时间戳
} __attribute__((packed)) video_param_t;

// OTA请求包参数结构体
typedef struct {
    uint32_t cmd;        // 0x09
    uint32_t subcmd;     // 0x02(请求包) 或 0x03(结束升级)
    uint32_t ota_size;   // OTA包大小
    char ota_name[32];    // OTA包名称/标识（8字节字符串）
} __attribute__((packed)) ota_request_t;

// OTA响应参数结构体
typedef struct {
    uint32_t cmd;        // 0x0A
    uint32_t status;     // 0x00(失败) 0x01(成功)
    uint32_t subcmd;     // 0x02(包状态) 0x03(重启状态)
    uint32_t result;     // 具体结果码
} __attribute__((packed)) ota_response_t;

static const uint32_t auto_spi_handshake_data[AUTO_SPI_HANDSHAKE_DATA_SIZE] = {COMMON_LINUX_HANDSHAKE, COMMON_HANDSHAKE_DATA};

// OTA包文件路径和相关变量
#define OTA_PACKAGE_PATH "/data/emmc0/velox/Athlics_20250617114955.bin"
static struct OTA_FILE_INFO_T cached_ota_info = {0};
static bool ota_info_loaded = false;
static int ota_package_fd = -1;
static uint32_t ota_package_size = 0;

/**
 * OTA固件传输模式兼容设计
 * 
 * 本系统提供两种OTA固件传输模式，用户可以根据需要动态切换：
 * 
 * 1. BLOCKING模式 (阻塞模式):
 *    - 使用osDelay()进行同步等待
 *    - 简单直接，容易理解和调试
 *    - 适用于测试环境或对实时性要求不高的场景
 *    - 函数：execute_ota_firmware_transfer_protocol()
 * 
 * 2. STATE_MACHINE模式 (状态机模式):
 *    - 使用定时器和事件驱动的非阻塞处理
 *    - 高效，避免阻塞主线程
 *    - 适用于生产环境或需要高实时性的场景
 *    - 函数：handle_ota_state_machine()
 * 
 * 使用方法：
 *    spictrl ota_mode        # 查看当前模式
 *    spictrl ota_mode 0      # 切换到阻塞模式
 *    spictrl ota_mode 1      # 切换到状态机模式
 * 
 * 注意事项：
 *    - 模式切换立即生效，下次OTA传输时使用新模式
 *    - 正在进行的传输不会受到影响
 *    - 默认使用状态机模式以获得最佳性能
 */

// OTA传输模式枚举
typedef enum {
    OTA_TRANSFER_MODE_BLOCKING = 0,    // 普通阻塞模式，使用osDelay同步等待
    OTA_TRANSFER_MODE_STATE_MACHINE = 1 // 状态机模式，使用定时器和事件驱动
} ota_transfer_mode_t;

// 全局OTA传输模式配置变量
static ota_transfer_mode_t g_ota_transfer_mode = OTA_TRANSFER_MODE_STATE_MACHINE; // 默认使用状态机模式



/***************************************************************************
 * Private Functions
 ****************************************************************************/

// 计算FCS校验和（简单的累加校验）
static uint32_t calculate_fcs(const ctl_fr_t *frame)
{
    uint32_t fcs = 0;
    const uint8_t *data = (const uint8_t *)frame;
    size_t len = sizeof(ctl_fr_t) - sizeof(uint32_t); // 不包括FCS字段本身
    
    for (size_t i = 0; i < len; i++) {
        fcs += data[i];
    }
    return fcs;
}

// 验证FCS校验和
static bool verify_fcs(const ctl_fr_t *frame)
{
    uint32_t calculated_fcs = calculate_fcs(frame);
    return (calculated_fcs == frame->fcs);
}

// 构建控制帧的通用函数
static int build_control_frame(ctl_fr_t *frame, uint8_t type, const uint32_t *data, size_t data_len, uint32_t seq_num)
{
    if (data_len > CTRL_MODE_DATA_LEN * sizeof(uint32_t)) {
        return -1; // 数据长度超出限制
    }
    
    memset(frame, 0, sizeof(ctl_fr_t));
    frame->fr_ctrl.ver = CMD_VERSION;
    frame->fr_ctrl.type = type;
    frame->fr_ctrl.retry_quit = 0x00;
    frame->fr_ctrl.reserved = 0x00;
    frame->seqctl = seq_num;
    frame->length = data_len;
    
    if (data && data_len > 0) {
        memcpy(frame->data, data, data_len);
    }
    
    frame->fcs = calculate_fcs(frame);
    return 0;
}

// 发送控制帧
static int send_control_frame(const ctl_fr_t *frame)
{
    return hal_user_spi_send(HAL_SPI_ID_0, (uint8_t*)frame, sizeof(ctl_fr_t));
}

// 接收控制帧（声明，实现在后面）
static int receive_control_frame(ctl_fr_t *frame);
static int execute_ota_protocol_from_step3(void);
static int execute_ota_firmware_transfer_protocol(void);
static int handle_ota_state_machine(void);

// 验证接收到的帧是否符合预期
static bool validate_received_frame(const ctl_fr_t *frame, uint32_t expected_cmd, size_t expected_min_len)
{
    if (frame->fr_ctrl.ver != CMD_VERSION) {
        TRACE(0, "spictrl: Photo Protocol: Invalid version 0x%02X", frame->fr_ctrl.ver);
        return false;
    }
    
    if (frame->length < expected_min_len) {
        TRACE(0, "spictrl: Photo Protocol: Invalid length %d, expected >= %d", frame->length, expected_min_len);
        return false;
    }
    
    if (frame->length > 0) {
        // 使用临时变量避免对packed结构体成员直接取地址
        uint32_t data0;
        memcpy(&data0, frame->data, sizeof(uint32_t));
        if (data0 != expected_cmd) {
            TRACE(0, "spictrl: Photo Protocol: Invalid command 0x%02X, expected 0x%02X", data0, expected_cmd);
            return false;
        }
    }
    
    return true;
}

// 自动识别业务类型的协议执行函数
static int execute_auto_business_protocol(void)
{
    static uint32_t auto_sequence_counter = 2000; // 协议序列号
    ctl_fr_t send_frame, recv_frame;
    int ret;
    business_type_t business_type = BUSINESS_TYPE_PHOTO; // 将在接收业务确认后确定
    const char *business_name = "Unknown";
    uint32_t business_data = 0;
    uint32_t param_cmd = 0;
    uint32_t success_cmd = 0;
    
    TRACE(0, "spictrl: Auto Business Protocol: Starting protocol execution...");
    
    // 步骤1: 发送Linux握手 [0xFE, 0x01]
    ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, auto_spi_handshake_data, 
                             AUTO_SPI_HANDSHAKE_DATA_SIZE, ++auto_sequence_counter);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to build Linux handshake frame", business_name);
        return -1;
    }
    
    ret = send_control_frame(&send_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: Auto Business Protocol: Failed to send Linux handshake, ret=%d", ret);
        return -2;
    }
    
    TRACE(0, "spictrl: Auto Business Protocol: Step 1 - Sent Linux handshake [0x%02X, 0x%02X], seq=%d", 
          COMMON_LINUX_HANDSHAKE, COMMON_HANDSHAKE_DATA, send_frame.seqctl);
    
    // 步骤2: 接收业务确认 [0xFD, 0x0A/0x03] 并自动识别业务类型
    osDelay(50); // 短暂延时等待响应
    ret = receive_control_frame(&recv_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: Auto Business Protocol: Failed to receive business confirm, ret=%d", ret);
        return -3;
    }
    
    if (!validate_received_frame(&recv_frame, COMMON_BUSINESS_CONFIRM, 2)) {
        return -4;
    }
    
    // 使用临时变量避免对packed结构体成员直接取地址
    uint32_t recv_data[2];
    memcpy(recv_data, recv_frame.data, sizeof(uint32_t) * 2);
    
    // 根据业务确认的第二个字节自动识别业务类型
    if (recv_data[1] == PHOTO_BUSINESS_ACK) {
        business_type = BUSINESS_TYPE_PHOTO;
        business_name = "Photo";
        business_data = PHOTO_BUSINESS_DATA;
        param_cmd = PHOTO_PARAM_CMD;
        success_cmd = PHOTO_SUCCESS_CMD;
        TRACE(0, "spictrl: Auto Business Protocol: Detected PHOTO business from ack 0x%02X", recv_data[1]);
    } else if (recv_data[1] == VIDEO_BUSINESS_ACK) {
        business_type = BUSINESS_TYPE_VIDEO;
        business_name = "Video";
        business_data = VIDEO_BUSINESS_DATA;
        param_cmd = VIDEO_PARAM_CMD;
        success_cmd = VIDEO_SUCCESS_CMD;
        TRACE(0, "spictrl: Auto Business Protocol: Detected VIDEO business from ack 0x%02X", recv_data[1]);
    } else if (recv_data[1] == OTA_BUSINESS_ACK) {
        // OTA业务类型特殊处理，直接调用OTA协议函数
        business_type = BUSINESS_TYPE_OTA;
        business_name = "OTA";
        TRACE(0, "spictrl: Auto Business Protocol: Detected OTA business from ack 0x%02X", recv_data[1]);
        
        // OTA协议与photo/video协议流程不同，需要在接收到OTA业务确认后重新开始
        TRACE(0, "spictrl: Auto Business Protocol: Switching to dedicated OTA protocol handler");
        
        // 重新构建OTA流程，从步骤3开始（已经完成了Linux握手和OTA业务确认）
        return execute_ota_protocol_from_step3();
    } else {
        TRACE(0, "spictrl: Auto Business Protocol: Unknown business type from ack 0x%02X", recv_data[1]);
        return -5;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 2 - Received business confirm [0x%02X, 0x%02X]", 
          business_name, recv_data[0], recv_data[1]);
    
    // 步骤3: 发送业务握手 [0xFE, 0x0A/0x03]
    uint32_t business_handshake_data[2] = {COMMON_LINUX_HANDSHAKE, business_data};
    ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, business_handshake_data, 
                             2*sizeof(uint32_t), ++auto_sequence_counter);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to build business handshake frame", business_name);
        return -6;
    }
    
    ret = send_control_frame(&send_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to send business handshake, ret=%d", business_name, ret);
        return -7;
    }
    TRACE(0, "spictrl: %s Protocol: Step 3 - Sent business handshake [0x%02X, 0x%02X], seq=%d", 
          business_name, COMMON_LINUX_HANDSHAKE, business_data, send_frame.seqctl);
    
    // 步骤4: 接收参数 [0x31/0x13, params...]
    osDelay(100); // 等待参数响应
    ret = receive_control_frame(&recv_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to receive parameters, ret=%d", business_name, ret);
        return -8;
    }
    
    if (!validate_received_frame(&recv_frame, param_cmd, 5)) {
        return -9;
    }
    
    if (business_type == BUSINESS_TYPE_PHOTO) {
        photo_param_t *photo_params = (photo_param_t*)recv_frame.data;
        TRACE(0, "spictrl: %s Protocol: Step 4 - Received photo params [0x%02X, num=%d, delay=%d, res=%d, time=%d]", 
              business_name, photo_params->cmd, photo_params->num, photo_params->delay, 
              photo_params->res, photo_params->time);
    } else {
        video_param_t *video_params = (video_param_t*)recv_frame.data;
        TRACE(0, "spictrl: %s Protocol: Step 4 - Received video params [0x%02X, duration=%d, delay=%d, res=%d, timestamp=%d]", 
              business_name, video_params->cmd, video_params->duration, video_params->delay, 
              video_params->res, video_params->timestamp);
    }
    
    // 步骤5: 发送成功确认 [0x32/0x14, 0x01]
    osDelay(100); 
    uint32_t success_data[2] = {success_cmd, PHOTO_SUCCESS_DATA}; // 注意：拍照和录像都使用0x01
    ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, success_data, 
                             2*sizeof(uint32_t), ++auto_sequence_counter);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to build success frame", business_name);
        return -10;
    }
    
    ret = send_control_frame(&send_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to send success confirm, ret=%d", business_name, ret);
        return -11;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 5 - Sent success confirm [0x%02X, 0x%02X], seq=%d", 
          business_name, success_cmd, PHOTO_SUCCESS_DATA, send_frame.seqctl);
    TRACE(0, "spictrl: %s Protocol: %s protocol completed successfully!", business_name, business_name);

    osDelay(100); // 等待参数响应
    ret = receive_control_frame(&recv_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to receive parameters, ret=%d", business_name, ret);
        return -8;
    }   

    // 可以在这里根据业务类型添加实际的拍照或录像逻辑
    if (business_type == BUSINESS_TYPE_PHOTO) {
        // 根据photo_params中的参数进行拍照操作
        TRACE(0, "spictrl: Photo Protocol: Execute photo capture logic here");
    } else {
        // 根据video_params中的参数进行录像操作
        TRACE(0, "spictrl: Video Protocol: Execute video recording logic here");
    }
    
    return 0;
}

// 自动识别业务类型的协议执行函数（包装）
static int execute_photo_protocol(void)
{
    return execute_auto_business_protocol();
}

// 自动识别业务类型的协议执行函数（包装）
static int execute_video_protocol(void)
{
    return execute_auto_business_protocol();
}


// 检查文件系统中是否存在指定的OTA包
// OTA传输模式配置接口
static void set_ota_transfer_mode(ota_transfer_mode_t mode)
{
    g_ota_transfer_mode = mode;
    TRACE(0, "spictrl: OTA transfer mode set to %s", 
          (mode == OTA_TRANSFER_MODE_BLOCKING) ? "BLOCKING" : "STATE_MACHINE");
}

static ota_transfer_mode_t get_ota_transfer_mode(void)
{
    return g_ota_transfer_mode;
}

static bool check_ota_package_exist(uint32_t ota_size, const char *ota_name)
{
    struct stat file_stat;
    
    // 确保ota_name字符串以NULL结尾（防止8字节刚好用完的情况）
    char safe_name[32] = {0};
    strncpy(safe_name, ota_name, sizeof(safe_name) - 1);
    safe_name[sizeof(safe_name) - 1] = '\0'; // 确保字符串以NULL结尾
    
    TRACE(0, "spictrl: OTA Protocol: Checking OTA package exist - name='%s', size=%d", safe_name, ota_size);
    
    // 使用文件系统API检查OTA包文件是否存在
    if (stat(OTA_PACKAGE_PATH, &file_stat) != 0) {
        TRACE(0, "spictrl: OTA Protocol: OTA package file not found: %s", OTA_PACKAGE_PATH);
        return false;
    }
    
    // 从文件路径中提取文件名
    const char *filename_start = strrchr(OTA_PACKAGE_PATH, '/');
    if (filename_start == NULL) {
        filename_start = OTA_PACKAGE_PATH; // 没有路径分隔符，整个字符串就是文件名
    } else {
        filename_start++; // 跳过路径分隔符
    }
    
    // 比较文件名和大小
    bool name_match = (strcmp(safe_name, filename_start) == 0);
    bool size_match = ((uint32_t)file_stat.st_size == ota_size);
    
    TRACE(0, "spictrl: OTA Protocol: File check - path: %s", OTA_PACKAGE_PATH);
    TRACE(0, "spictrl: OTA Protocol: Expected: name='%s', size=%d", safe_name, ota_size);
    TRACE(0, "spictrl: OTA Protocol: Actual:   name='%s', size=%d", filename_start, (uint32_t)file_stat.st_size);
    TRACE(0, "spictrl: OTA Protocol: Match result: name=%s, size=%s", 
          name_match ? "YES" : "NO", size_match ? "YES" : "NO");
    
    if (name_match && size_match) {
        TRACE(0, "spictrl: OTA Protocol: OTA package found and matches - '%s' (%d bytes)", 
              filename_start, (uint32_t)file_stat.st_size);
        return true;
    }
    
    TRACE(0, "spictrl: OTA Protocol: OTA package mismatch - expected '%s' (%d bytes), found '%s' (%d bytes)", 
          safe_name, ota_size, filename_start, (uint32_t)file_stat.st_size);
    return false;
}

// 计算CRC32校验和
static uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
    // 简化的CRC32计算，实际项目中应使用标准CRC32算法
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

// 打开OTA包文件
static int open_ota_package_file(void)
{
    struct stat file_stat;
    
    if (ota_package_fd >= 0) {
        return 0; // 已经打开
    }
    
    // 检查文件是否存在
    if (stat(OTA_PACKAGE_PATH, &file_stat) != 0) {
        TRACE(0, "spictrl: OTA: Package file not found: %s", OTA_PACKAGE_PATH);
        return -1;
    }
    
    ota_package_size = file_stat.st_size;
    TRACE(0, "spictrl: OTA: Package file size: %d bytes", ota_package_size);
    
    // 打开文件
    ota_package_fd = open(OTA_PACKAGE_PATH, O_RDONLY);
    if (ota_package_fd < 0) {
        TRACE(0, "spictrl: OTA: Failed to open package file: %s", OTA_PACKAGE_PATH);
        return -2;
    }
    
    TRACE(0, "spictrl: OTA: Package file opened successfully: %s", OTA_PACKAGE_PATH);
    return 0;
}

// 关闭OTA包文件
static void close_ota_package_file(void)
{
    if (ota_package_fd >= 0) {
        close(ota_package_fd);
        ota_package_fd = -1;
        TRACE(0, "spictrl: OTA: Package file closed");
    }
}

// 计算文件在OTA包中的数据偏移量
static uint32_t calculate_file_data_offset(int file_index)
{
    uint32_t offset = sizeof(struct OTA_FILE_INFO_T); // 跳过文件信息头
    
    // 累加前面所有文件的大小
    for (int i = 0; i < file_index; i++) {
        offset += cached_ota_info.file_length[i];
    }
    
    return offset;
}

// 从OTA包文件中加载文件信息
static int load_ota_file_info(struct OTA_FILE_INFO_T *ota_info)
{
    int ret;
    ssize_t bytes_read;
    uint32_t calculated_crc32;
    
    // 如果已经加载过，直接返回缓存的信息
    if (ota_info_loaded) {
        memcpy(ota_info, &cached_ota_info, sizeof(struct OTA_FILE_INFO_T));
        TRACE(0, "spictrl: OTA: Using cached file info - %d files", ota_info->file_numbers);
        return 0;
    }
    
    // 打开OTA包文件
    ret = open_ota_package_file();
    if (ret != 0) {
        TRACE(0, "spictrl: OTA: Failed to open package file, ret=%d", ret);
        return ret;
    }
    
    // 检查文件大小是否足够包含文件信息头
    if (ota_package_size < sizeof(struct OTA_FILE_INFO_T)) {
        TRACE(0, "spictrl: OTA: Package file too small: %d < %d", 
              ota_package_size, sizeof(struct OTA_FILE_INFO_T));
        close_ota_package_file();
        return -3;
    }
    
    // 读取文件开头的OTA_FILE_INFO_T结构
    lseek(ota_package_fd, 0, SEEK_SET);
    bytes_read = read(ota_package_fd, &cached_ota_info, sizeof(struct OTA_FILE_INFO_T));
    if (bytes_read != sizeof(struct OTA_FILE_INFO_T)) {
        TRACE(0, "spictrl: OTA: Failed to read file info, expected=%d, actual=%d", 
              sizeof(struct OTA_FILE_INFO_T), bytes_read);
        close_ota_package_file();
        return -4;
    }
    
    // 校验文件信息的CRC32
    calculated_crc32 = calculate_crc32((uint8_t*)&cached_ota_info, 
                                      sizeof(struct OTA_FILE_INFO_T) - sizeof(uint32_t));
    if (calculated_crc32 != cached_ota_info.crc32) {
        TRACE(0, "spictrl: OTA: File info CRC32 verification failed");
        TRACE(0, "spictrl: OTA: Calculated=0x%08X, Expected=0x%08X", 
              calculated_crc32, cached_ota_info.crc32);
        close_ota_package_file();
        return -5;
    }
    
    // 验证文件数量
    if (cached_ota_info.file_numbers == 0 || 
        cached_ota_info.file_numbers > LWK_OTA_BIN_MAX) {
        TRACE(0, "spictrl: OTA: Invalid file numbers: %d", cached_ota_info.file_numbers);
        close_ota_package_file();
        return -6;
    }
    
    // 验证文件大小合理性
    uint32_t total_file_size = sizeof(struct OTA_FILE_INFO_T);
    for (uint32_t i = 0; i < cached_ota_info.file_numbers; i++) {
        total_file_size += cached_ota_info.file_length[i];
        
        // 检查单个文件大小是否合理
        if (cached_ota_info.file_length[i] == 0 || 
            cached_ota_info.file_length[i] > 10*1024*1024) { // 最大10MB
            TRACE(0, "spictrl: OTA: Invalid file[%d] size: %d", i, cached_ota_info.file_length[i]);
            close_ota_package_file();
            return -7;
        }
    }
    
    if (total_file_size > ota_package_size) {
        TRACE(0, "spictrl: OTA: Package size mismatch, expected=%d, actual=%d", 
              total_file_size, ota_package_size);
        close_ota_package_file();
        return -8;
    }
    
    // 复制到输出参数
    memcpy(ota_info, &cached_ota_info, sizeof(struct OTA_FILE_INFO_T));
    ota_info_loaded = true;
    
    // 打印文件信息
    TRACE(0, "spictrl: OTA: Successfully loaded file info from package");
    TRACE(0, "spictrl: OTA: Package: %s, Size: %d bytes", OTA_PACKAGE_PATH, ota_package_size);
    TRACE(0, "spictrl: OTA: File numbers: %d, CRC32: 0x%08X", 
          ota_info->file_numbers, ota_info->crc32);
    
    for (uint32_t i = 0; i < ota_info->file_numbers; i++) {
        TRACE(0, "spictrl: OTA: File[%d]: %s, type=%d, addr=0x%08X, size=%d", 
              i, ota_info->file_name[i], ota_info->file_type[i], 
              ota_info->file_start_addr[i], ota_info->file_length[i]);
    }
    
    return 0;
}

// 从OTA包文件中读取指定文件的数据
static int read_ota_file_data(const char *filename, uint32_t offset, 
                             struct OTA_FILE_DATA_T *ota_data)
{
    int file_index = -1;
    uint32_t file_data_offset;
    uint32_t read_size;
    ssize_t bytes_read;
    
    // 清空数据缓冲区
    memset(ota_data, 0, sizeof(struct OTA_FILE_DATA_T));
    
    // 确保文件信息已加载
    if (!ota_info_loaded) {
        TRACE(0, "spictrl: OTA: File info not loaded");
        return -1;
    }
    
    // 确保文件已打开
    if (ota_package_fd < 0) {
        TRACE(0, "spictrl: OTA: Package file not opened");
        return -2;
    }
    
    // 根据文件名查找文件索引
    for (uint32_t i = 0; i < cached_ota_info.file_numbers; i++) {
        if (strcmp((char*)cached_ota_info.file_name[i], filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    if (file_index < 0) {
        TRACE(0, "spictrl: OTA: File not found in package: %s", filename);
        return -3;
    }
    
    // 检查偏移量是否超出文件大小
    if (offset >= cached_ota_info.file_length[file_index]) {
        TRACE(0, "spictrl: OTA: Offset %d exceeds file size %d for %s", 
              offset, cached_ota_info.file_length[file_index], filename);
        return -4;
    }
    
    // 计算要读取的数据大小
    uint32_t remaining_in_file = cached_ota_info.file_length[file_index] - offset;
    read_size = (remaining_in_file > (SPICOMM_LINKLAYER_DATA_SIZE-4)) ? 
                (SPICOMM_LINKLAYER_DATA_SIZE-4) : remaining_in_file;
    
    // 计算在OTA包中的绝对偏移量
    file_data_offset = calculate_file_data_offset(file_index) + offset;
    
    TRACE(0, "spictrl: OTA: Reading file[%d] '%s' at offset %d, size %d (package_offset=%d)", 
          file_index, filename, offset, read_size, file_data_offset);
    
    // 定位到正确的位置并读取数据
    if (lseek(ota_package_fd, file_data_offset, SEEK_SET) != file_data_offset) {
        TRACE(0, "spictrl: OTA: Failed to seek to offset %d", file_data_offset);
        return -5;
    }
    
    bytes_read = read(ota_package_fd, ota_data->file_data, read_size);
    if (bytes_read != read_size) {
        TRACE(0, "spictrl: OTA: Failed to read file data, expected=%d, actual=%d", 
              read_size, bytes_read);
        return -6;
    }
    DUMP8("%02x ", ota_data->file_data, 10);
    // 计算数据的CRC32
    ota_data->crc32 = calculate_crc32((uint8_t*)ota_data->file_data, read_size);
    
    TRACE(0, "spictrl: OTA: Successfully read %d bytes, CRC32=0x%08X", read_size, ota_data->crc32);
    
    return read_size; // 返回实际读取的数据长度
}

// 统一的OTA固件传输入口函数，直接使用阻塞模式
static int execute_ota_firmware_transfer(void)
{
    TRACE(0, "spictrl: Starting OTA firmware transfer using BLOCKING mode");
    
    // 直接使用阻塞模式传输（已消除osDelay调用）
    return execute_ota_firmware_transfer_protocol();
}

// OTA固件包传送协议执行函数（2700已在OTA boot状态下的固件传送）
static int execute_ota_firmware_transfer_protocol(void)
{
    static uint32_t ota_sequence_counter = 4100; // 固件传送协议序列号
    ctl_fr_t send_frame, recv_frame;
    int ret;
    const char *protocol_name = "OTA_FIRMWARE";
    
    TRACE(0, "spictrl: %s Protocol: Starting OTA firmware transfer protocol (BES2700 in OTA boot mode)...", protocol_name);
    
    // 步骤1: 发送OTA升级锁 [0x0A, 0x01, 0x03, 0x01]
    uint32_t upgrade_lock_data[4] = {0x0A, 0x01, 0x03, 0x01};
    ret = build_control_frame(&send_frame, OTA_SPI_CMD, upgrade_lock_data, 
                             4*sizeof(uint32_t), ++ota_sequence_counter);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to build upgrade lock frame", protocol_name);
        return -3;
    }
    
    ret = send_control_frame(&send_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to send upgrade lock, ret=%d", protocol_name, ret);
        return -4;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 1 - Sent OTA upgrade lock [0x0A, 0x01, 0x03, 0x01]", protocol_name);
    
    // 步骤2: 接收空包响应 [0x00, 0x00]
    osDelay(100);
    ret = receive_control_frame(&recv_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to receive empty response, ret=%d", protocol_name, ret);
        return -5;
    }
    
    if (!validate_received_frame(&recv_frame, 0x00, 2)) {
        TRACE(0, "spictrl: %s Protocol: Invalid empty response received", protocol_name);
        return -6;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 2 - Received empty response [0x00, 0x00]", protocol_name);
    
    // 步骤3: 发送OTA包准备锁 [0x0A, 0x00, 0x00, 0x01]
    uint32_t package_lock_data[4] = {0x0A, 0x00, 0x00, 0x01};
    ret = build_control_frame(&send_frame, OTA_SPI_CMD, package_lock_data, 
                             4*sizeof(uint32_t), ++ota_sequence_counter);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to build package lock frame", protocol_name);
        return -7;
    }
    
    ret = send_control_frame(&send_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to send package lock, ret=%d", protocol_name, ret);
        return -8;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 3 - Sent OTA package lock [0x0A, 0x00, 0x00, 0x01]", protocol_name);
    
    // 步骤4: 接收OTA包请求 [0x09, 0x01]
    osDelay(100);
    ret = receive_control_frame(&recv_frame);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to receive package request, ret=%d", protocol_name, ret);
        return -9;
    }
    
    if (!validate_received_frame(&recv_frame, 0x09, 2)) {
        TRACE(0, "spictrl: %s Protocol: Invalid package request received", protocol_name);
        return -10;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 4 - Received OTA package request [0x09, 0x01]", protocol_name);
    
    // 步骤5: 通过spibuf[1028]发送OTA包头[struct OTA_FILE_INFO_T]
    static struct OTA_FILE_INFO_T ota_file_info; // 改为静态变量减少栈使用
    static uint8_t spi_buffer[SPICOMM_LINKLAYER_DATA_SIZE]; // 改为静态变量减少栈使用
    
    ret = load_ota_file_info(&ota_file_info);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to load OTA file info, ret=%d", protocol_name, ret);
        return -11;
    }
    
    // 将OTA文件信息拷贝到SPI缓冲区
    memset(spi_buffer, 0, sizeof(spi_buffer));
    memcpy(spi_buffer, &ota_file_info, sizeof(struct OTA_FILE_INFO_T));
    
    // 注意：spibuf不使用ctl_fr_t封包，直接发送原始数据
    ret = hal_user_spi_send(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to send OTA file info via spibuf, ret=%d", protocol_name, ret);
        return -12;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 5 - Sent OTA file info via spibuf[1028], %d files", 
          protocol_name, ota_file_info.file_numbers);
    
    // 步骤6: 通过spibuf[1028]接收OTA包头回复[struct OTA_FILE_INFO_T]
    osDelay(100);
    ret = hal_user_spi_recv_only(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
    if (ret != 0) {
        TRACE(0, "spictrl: %s Protocol: Failed to receive OTA file info reply via spibuf, ret=%d", protocol_name, ret);
        return -13;
    }
    
    // 校验返回的OTA文件信息
    struct OTA_FILE_INFO_T *recv_ota_info = (struct OTA_FILE_INFO_T*)spi_buffer;
    
    // 重新计算接收到的文件信息的CRC32
    uint32_t recv_calculated_crc32 = calculate_crc32((uint8_t*)recv_ota_info, 
                                                     sizeof(struct OTA_FILE_INFO_T) - sizeof(uint32_t));
    
    // 校验接收到的包的数据完整性
    if (recv_calculated_crc32 != recv_ota_info->crc32) {
        TRACE(0, "spictrl: %s Protocol: Received OTA file info CRC32 verification failed", protocol_name);
        TRACE(0, "spictrl: %s Protocol: Calculated CRC32=0x%08X, Received CRC32=0x%08X", 
              protocol_name, recv_calculated_crc32, recv_ota_info->crc32);
        return -14;
    }
    
    // 校验文件数量是否匹配
    if (recv_ota_info->file_numbers != ota_file_info.file_numbers) {
        TRACE(0, "spictrl: %s Protocol: File numbers mismatch, expected=%d, received=%d", 
              protocol_name, ota_file_info.file_numbers, recv_ota_info->file_numbers);
        return -15;
    }
    
    TRACE(0, "spictrl: %s Protocol: Step 6 - Received OTA file info reply via spibuf[1028], verification passed", protocol_name);
    
    // 步骤7-8: 循环发送各个文件的数据包
    for (uint32_t file_idx = 0; file_idx < ota_file_info.file_numbers; file_idx++) {
        uint32_t file_length = ota_file_info.file_length[file_idx];
        uint32_t bytes_sent = 0;
        const char *filename = (const char*)ota_file_info.file_name[file_idx];
        
        TRACE(0, "spictrl: %s Protocol: Starting transfer of file %d: '%s' (%d bytes)", 
              protocol_name, file_idx, filename, file_length);
        hal_sysfreq_req(HAL_SYSFREQ_USER_APP_0, HAL_CMU_FREQ_208M);
        
        while (bytes_sent < file_length) 
        {
            static struct OTA_FILE_DATA_T ota_file_data; // 改为静态变量减少栈使用
            static uint32_t ota_file_data_timeout_count = 0;
            if (ota_state_timer_id) 
            {
                osTimerStart(ota_state_timer_id, 100);
            }
            uint32_t flags = osEventFlagsWait(gpio_spi_event_flags, SPI_EVENT_TIMEOUT, osFlagsWaitAny, osWaitForever);
            if(flags&SPI_EVENT_TIMEOUT)
            {
                // 读取文件数据
                int data_len = read_ota_file_data(filename, bytes_sent, &ota_file_data);
                if (data_len <= 0) {
                    TRACE(0, "spictrl: %s Protocol: Failed to read file data, ret=%d", protocol_name, data_len);
                    return -15;
                }
                ota_file_data_timeout_count++;

                // 调整数据长度，不超过剩余文件大小
                uint32_t remaining = file_length - bytes_sent;
                if (data_len > remaining) {
                    data_len = remaining;
                }
                
                // 步骤7: 通过spibuf[1028]发送OTA数据包[struct OTA_FILE_DATA_T]
                memset(spi_buffer, 0, sizeof(spi_buffer));
                memcpy(spi_buffer, &ota_file_data, sizeof(struct OTA_FILE_DATA_T));
                DUMP8("%02x ", spi_buffer, 10);


                ret = hal_user_spi_send(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                if (ret != 0) {
                    TRACE(0, "spictrl: %s Protocol: Failed to send file data via spibuf, ret=%d", protocol_name, ret);
                    return -16;
                }
                
                TRACE(0, "spictrl: %s Protocol: Step 7 - Sent file data via spibuf[1028], offset=%d, length=%d", 
                    protocol_name, bytes_sent, data_len);
                
                // 步骤8: 通过spibuf[1028]接收OTA数据包回复[struct OTA_FILE_DATA_T]
                ret = hal_user_spi_recv_only(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                if (ret != 0) {
                    TRACE(0, "spictrl: %s Protocol: Failed to receive file data reply via spibuf, ret=%d", protocol_name, ret);
                    return -17;
                }
                
                // 校验返回的数据包
                struct OTA_FILE_DATA_T *recv_data = (struct OTA_FILE_DATA_T*)spi_buffer;
                
                // 重新计算接收到的文件数据的CRC32
                uint32_t recv_calculated_crc32 = calculate_crc32((uint8_t*)recv_data->file_data, 
                                                                SPICOMM_LINKLAYER_DATA_SIZE-4);
                
                // 校验接收到的包的数据完整性
                DUMP8("%02x ", recv_data->file_data, 10);
                if (recv_calculated_crc32 != recv_data->crc32) {
                    TRACE(0, "spictrl: %s Protocol: Received file data CRC32 verification failed, retransmission needed", protocol_name);
                    TRACE(0, "spictrl: %s Protocol: Calculated CRC32=0x%08X, Received CRC32=0x%08X", 
                        protocol_name, recv_calculated_crc32, recv_data->crc32);
                    // 重发当前数据包
                    continue;
                }
                
                // 进一步校验：对比发送和接收的数据是否一致
                if (memcmp(ota_file_data.file_data, recv_data->file_data, data_len) != 0) {
                    TRACE(0, "spictrl: %s Protocol: File data content mismatch, retransmission needed", protocol_name);
                    // 重发当前数据包
                    continue;
                }
                
                TRACE(0, "spictrl: %s Protocol: Step 8 - Received file data reply via spibuf[1028], verification passed", protocol_name);
                if(ota_file_data_timeout_count%10==0)
                {
                    struct HAL_SPI_CFG_T spi_cfg = {
                        .rate = 10000000,        // 10MHz
                        .clk_delay_half = true,
                        .clk_polarity = true,
                        .slave = false,
                        .dma_rx = false,
                        .dma_tx = false,
                        .cs = 0,
                        .tx_bits = 8,
                        .rx_bits = 8,
                        .samp_delay = 0,
                    };
                    int ret;

                    hal_user_spi_close(HAL_SPI_ID_0, 0);
                    osDelay(200);
                    hal_iomux_set_spi();
                    ret = hal_user_spi_open(HAL_SPI_ID_0, &spi_cfg);
                    if (ret == 0) {
                        TRACE(0, "spictrl: OTA Firmware Transfer: SPI initialized successfully");
                    }                    
                }
                bytes_sent += data_len;
                
                // 显示传输进度
                uint32_t progress = (bytes_sent * 100) / file_length;
                TRACE(0, "spictrl: %s Protocol: File '%s' transfer progress: %d%% (%d/%d bytes)", 
                    protocol_name, filename, progress, bytes_sent, file_length);
                
                TRACE(0, "spictrl: %s Protocol: File %d '%s' transfer completed successfully", 
                protocol_name, file_idx, filename);
                TRACE(0,"CNT %d", ota_file_data_timeout_count);
            }   
        }
    }
    
    TRACE(0, "spictrl: %s Protocol: All OTA files transferred successfully!", protocol_name);
    TRACE(0, "spictrl: %s Protocol: bes2700 should now process the received firmware packages", protocol_name);
    
    // 关闭OTA包文件
    close_ota_package_file();
    
    return 0; // 成功完成OTA固件包传送流程
}

// OTA协议执行函数（从步骤3开始，用于自动识别业务类型后）- 原有逻辑，用于让应用固件进入OTA boot
static int execute_ota_protocol_from_step3(void)
{
    static uint32_t ota_sequence_counter = 3100; // OTA协议序列号
    ctl_fr_t send_frame, recv_frame;
    int ret;
    const char *protocol_name = "OTA";
    const int max_wait_cycles = 300; // 最多等待300个周期（300秒=5分钟）
    int wait_cycle = 0;
    
    TRACE(0, "spictrl: %s Protocol: Starting OTA upgrade protocol from step 3...", protocol_name);
    
    // 步骤3-5: 循环发送空包并等待接收OTA请求
    while (wait_cycle < max_wait_cycles) 
    {
        // 发送空包 [0x00, 0x00]
        uint32_t empty_data[2] = {OTA_EMPTY_CMD, OTA_EMPTY_DATA};
        ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, empty_data, 
                                 2*sizeof(uint32_t), ++ota_sequence_counter);
        if (ret != 0) {
            TRACE(0, "spictrl: %s Protocol: Failed to build empty frame", protocol_name);
            return -6;
        }
        
        TRACE(0, "spictrl: %s Protocol: Cycle %d - Sent empty frame [0x%02X, 0x%02X], waiting for OTA request...", 
              protocol_name, wait_cycle + 1, OTA_EMPTY_CMD, OTA_EMPTY_DATA);
        
        // 等待GPIO中断事件或停止事件
        uint32_t flags = osEventFlagsWait(gpio_spi_event_flags, 
                                         GPIO_SPI_TRIGGER_EVENT | GPIO_SPI_STOP_EVENT,
                                         osFlagsWaitAny, osWaitForever);
        
        if (flags & GPIO_SPI_TRIGGER_EVENT) 
        {
            wait_cycle = 300;
            if (!gpio_auto_enabled) 
            {
                continue; // 如果已禁用，继续等待
            }
            osDelay(100);
            ret = send_control_frame(&send_frame);
            if (ret != 0) {
                TRACE(0, "spictrl: %s Protocol: Failed to send empty frame, ret=%d", protocol_name, ret);
                return -7;
            }   
        end_ota: 
            osDelay(100);
            // 尝试接收OTA请求
            ret = receive_control_frame(&recv_frame);
            if (ret == 0) {
                // 成功接收到数据，检查是否是OTA请求
                if (recv_frame.length >= sizeof(ota_request_t) && 
                    validate_received_frame(&recv_frame, OTA_REQUEST_CMD, sizeof(ota_request_t))) {
                    
                    ota_request_t *ota_request = (ota_request_t*)recv_frame.data;
                    
                    TRACE(0, "spictrl: %s Protocol: Received OTA request [cmd=0x%02X, subcmd=0x%02X, size=%d, name=%s]", 
                        protocol_name, ota_request->cmd, ota_request->subcmd, 
                        ota_request->ota_size, ota_request->ota_name);
                    
                    if (ota_request->subcmd == OTA_REQUEST_PACKAGE) {
                        // 检查文件系统中是否存在对应的OTA包
                        // 确保字符串安全处理
                        char safe_ota_name[32] = {0};
                        strncpy(safe_ota_name, ota_request->ota_name, sizeof(safe_ota_name) - 1);
                        safe_ota_name[sizeof(safe_ota_name) - 1] = '\0'; // 确保字符串以NULL结尾
                        
                        bool package_exist = check_ota_package_exist(ota_request->ota_size, safe_ota_name);
                        
                        // 构建响应 [0x0A, 0x01, 0x02, 0x00/0x01]
                        ota_response_t ota_response = {
                            .cmd = OTA_RESPONSE_CMD,           // 0x0A
                            .status = OTA_RESPONSE_SUCCESS,    // 0x01
                            .subcmd = 0x02,                    // 0x02 (包状态检查)
                            .result = package_exist ? 0x00 : 0x01  // 0x00=存在, 0x01=不存在
                        };
                        
                        // 复制到对齐的缓冲区以避免packed结构体对齐警告
                        uint32_t response_buffer[sizeof(ota_response_t) / sizeof(uint32_t) + 1] = {0};
                        memcpy(response_buffer, &ota_response, sizeof(ota_response));
                        
                        ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, response_buffer, 
                                                sizeof(ota_response), ++ota_sequence_counter);
                        if (ret != 0) {
                            TRACE(0, "spictrl: %s Protocol: Failed to build OTA response frame", protocol_name);
                            return -8;
                        }
                        osDelay(100);
                        ret = send_control_frame(&send_frame);
                        if (ret != 0) {
                            TRACE(0, "spictrl: %s Protocol: Failed to send OTA response, ret=%d", protocol_name, ret);
                            return -9;
                        }
                        
                        TRACE(0, "spictrl: %s Protocol: Sent OTA response [0x%02X, 0x%02X, 0x%02X, 0x%02X] - Package %s", 
                            protocol_name, ota_response.cmd, ota_response.status, 
                            ota_response.subcmd, ota_response.result,
                            package_exist ? "EXISTS" : "NOT FOUND");
                        
                        if (!package_exist) {
                            TRACE(0, "spictrl: %s Protocol: OTA package not found, exiting upgrade process", protocol_name);
                            goto end_ota;
                        }
                        
                        // OTA包存在，继续下一步流程
                    } else if (ota_request->subcmd == OTA_REQUEST_END) {
                        TRACE(0, "spictrl: %s Protocol: Received end upgrade command, terminating OTA process", protocol_name);
                        return -11; // 接收到结束升级命令
                    }
                }
            }
            osDelay(100);
            ret = receive_control_frame(&recv_frame);
            if (ret != 0) {
                TRACE(0, "spictrl: %s Protocol: Failed to receive second OTA business confirm, ret=%d", protocol_name, ret);
                return -17;
            }

            osDelay(100);

            
            // 构建OTA boot指令
            ota_response_t boot_command = {
                .cmd = OTA_RESPONSE_CMD,        // 0x0A
                .status = OTA_RESPONSE_SUCCESS, // 0x01
                .subcmd = OTA_RESPONSE_RESTART, // 0x03
                .result = 0x01                  // 0x01
            };
            
            // 复制到对齐的缓冲区以避免packed结构体对齐警告
            uint32_t boot_buffer[sizeof(ota_response_t) / sizeof(uint32_t) + 1] = {0};
            memcpy(boot_buffer, &boot_command, sizeof(boot_command));
            
            ret = build_control_frame(&send_frame, NORMAL_SPI_CMD, boot_buffer, 
                                    sizeof(boot_command), ++ota_sequence_counter);
            if (ret != 0) {
                TRACE(0, "spictrl: %s Protocol: Failed to build OTA boot command frame", protocol_name);
                return -19;
            }
            
            ret = send_control_frame(&send_frame);
            if (ret != 0) {
                TRACE(0, "spictrl: %s Protocol: Failed to send OTA boot command, ret=%d", protocol_name, ret);
                return -20;
            }
            
            TRACE(0, "spictrl: %s Protocol: Step 9 - Sent OTA boot command [0x%02X, 0x%02X, 0x%02X, 0x%02X] to bes2700", 
                protocol_name, boot_command.cmd, boot_command.status, boot_command.subcmd, boot_command.result);
            
            TRACE(0, "spictrl: %s Protocol: OTA upgrade protocol completed successfully! bes2700 should restart into OTA boot mode.", protocol_name);
            TRACE(0, "spictrl: %s Protocol: bes2700 will now enter OTA boot state for actual package reception", protocol_name);
        }
        osDelay(100);
        ret = receive_control_frame(&recv_frame);
        if (ret != 0) {
            TRACE(0, "spictrl: %s Protocol: Failed to receive second OTA business confirm, ret=%d", protocol_name, ret);
            return -17;
        }
        // 没有接收到OTA请求，继续循环
        wait_cycle++;
        if (wait_cycle % 60 == 0) {  // 每分钟打印一次状态
            TRACE(0, "spictrl: %s Protocol: Still waiting for OTA request... (%d/%d cycles)", 
                protocol_name, wait_cycle, max_wait_cycles);
        }        
        if (wait_cycle >= max_wait_cycles) {
            TRACE(0, "spictrl: %s Protocol: Timeout waiting for OTA request (5 minutes), exiting upgrade process", protocol_name);
            return -12; // 超时退出
        }
    }
    return 0; // 成功完成OTA升级流程
}

// GPIO中断处理函数
static void gpio_spi_irq_handler(enum HAL_GPIO_PIN_T pin)
{
    if (gpio_spi_event_flags != NULL && pin == auto_gpio_pin) {
            // 检查GPIO06的电平状态来决定业务类型
            int gpio06_value = hal_gpio_pin_get_val(HAL_GPIO_PIN_P0_6);
            if (gpio06_value) {
                // GPIO06高电平：OTA固件传送事件（2700已在boot状态）
                TRACE(0, "spictrl: GPIO Auto SPI: GPIO_SPI_UBOOT_DET_EVENT (GPIO06 HIGH - OTA firmware transfer)");
                osEventFlagsSet(gpio_spi_event_flags, GPIO_SPI_UBOOT_DET_EVENT);
            } else {
                // GPIO06低电平：普通触发事件
                TRACE(0, "spictrl: GPIO Auto SPI: GPIO_SPI_TRIGGER_EVENT (GPIO06 LOW - normal business)");
                osEventFlagsSet(gpio_spi_event_flags, GPIO_SPI_TRIGGER_EVENT);
            }
    }
}

// 简化的SPI接收函数 - 阻塞式，无DMA，使用控制帧格式
static int spi_simple_recv(enum HAL_SPI_ID_T spi_id, ctl_fr_t *recv_frame)
{
    // 清空接收缓冲区
    memset(recv_frame, 0x00, sizeof(ctl_fr_t));
    
    // 使用普通阻塞式接收控制帧
    int ret = hal_user_spi_recv_only(spi_id, (uint8_t*)recv_frame, sizeof(ctl_fr_t));
    
    return ret;
}

// 接收控制帧的实现
static int receive_control_frame(ctl_fr_t *frame)
{
    int ret = spi_simple_recv(HAL_SPI_ID_0, frame);
    if (ret == 0) {
        if (!verify_fcs(frame)) {
            TRACE(0, "spictrl: Photo Protocol: Received frame with invalid FCS");
            return -1;
        }
    }
    return ret;
}



// 定时器回调函数 - 只设置事件，不执行阻塞操作
static void ota_state_timer_callback(void *arg)
{
    // 只设置超时事件，不执行任何阻塞操作
    if (gpio_spi_event_flags != NULL) {
        osEventFlagsSet(gpio_spi_event_flags, SPI_EVENT_TIMEOUT);
    }
}

// 非阻塞的状态处理函数
static int handle_ota_state_machine(void)
{
    static uint32_t ota_sequence_counter = 4100;
    static uint8_t spi_buffer[SPICOMM_LINKLAYER_DATA_SIZE];
    ctl_fr_t send_frame, recv_frame;
    int ret = 0;
    
    switch (ota_current_state) {
        case OTA_STATE_IDLE:
        {
            // 开始OTA升级锁发送
            uint32_t upgrade_lock_data[4] = {0x0A, 0x01, 0x03, 0x01};
            ret = build_control_frame(&send_frame, OTA_SPI_CMD, upgrade_lock_data, 
                                     4*sizeof(uint32_t), ++ota_sequence_counter);
            if (ret == 0) {
                ret = send_control_frame(&send_frame);
                if (ret == 0) {
                    ota_current_state = OTA_STATE_UPGRADE_LOCK_SENT;
                    // 启动超时定时器，100ms后检查
                    if (ota_state_timer_id) {
                        osTimerStart(ota_state_timer_id, 10);
                    }
                    TRACE(0, "spictrl: OTA Protocol: Step 1 - Sent OTA upgrade lock");
                }
            }
        }
        break;
            
        case OTA_STATE_UPGRADE_LOCK_SENT:
            // 尝试接收空包响应
            ret = receive_control_frame(&recv_frame);
            if (ret == 0) {
                if (validate_received_frame(&recv_frame, 0x00, 2)) {
                    ota_current_state = OTA_STATE_EMPTY_RESPONSE_WAIT;
                    TRACE(0, "spictrl: OTA Protocol: Step 2 - Received empty response");
                    // 继续下一步，发送包锁
                    uint32_t package_lock_data[4] = {0x0A, 0x00, 0x00, 0x01};
                    ret = build_control_frame(&send_frame, OTA_SPI_CMD, package_lock_data, 
                                             4*sizeof(uint32_t), ++ota_sequence_counter);
                    if (ret == 0) {
                        ret = send_control_frame(&send_frame);
                        if (ret == 0) {
                            ota_current_state = OTA_STATE_PACKAGE_LOCK_SENT;
                            if (ota_state_timer_id) {
                                osTimerStart(ota_state_timer_id, 10);
                            }
                            TRACE(0, "spictrl: OTA Protocol: Step 3 - Sent OTA package lock");
                        }
                    }
                } else {
                    TRACE(0, "spictrl: OTA Protocol: Invalid empty response received");
                    ret = -1;
                }
            }
            // 如果没有收到数据，等待下次调用
            break;
            
        case OTA_STATE_PACKAGE_LOCK_SENT:
            // 尝试接收OTA包请求
            ret = receive_control_frame(&recv_frame);
            if (ret == 0) {
                if (validate_received_frame(&recv_frame, 0x09, 2)) {
                    ota_current_state = OTA_STATE_PACKAGE_REQUEST_WAIT;
                    TRACE(0, "spictrl: OTA Protocol: Step 4 - Received OTA package request");
                    
                    // 准备发送文件信息
                    ret = load_ota_file_info(&ota_file_info);
                    if (ret == 0) {
                        memset(spi_buffer, 0, sizeof(spi_buffer));
                        memcpy(spi_buffer, &ota_file_info, sizeof(struct OTA_FILE_INFO_T));
                        
                        ret = hal_user_spi_send(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                        if (ret == 0) {
                            ota_current_state = OTA_STATE_FILE_INFO_SENDING;
                            if (ota_state_timer_id) {
                                osTimerStart(ota_state_timer_id, 10);
                            }
                            TRACE(0, "spictrl: OTA Protocol: Step 5 - Sent OTA file info");
                        }
                    }
                } else {
                    TRACE(0, "spictrl: OTA Protocol: Invalid package request received");
                    ret = -1;
                }
            }
            break;
            
        case OTA_STATE_FILE_INFO_SENDING:
            // 接收文件信息回复
            ret = hal_user_spi_recv_only(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
            if (ret == 0) {
                struct OTA_FILE_INFO_T *recv_ota_info = (struct OTA_FILE_INFO_T*)spi_buffer;
                uint32_t recv_calculated_crc32 = calculate_crc32((uint8_t*)recv_ota_info, 
                                                                 sizeof(struct OTA_FILE_INFO_T) - sizeof(uint32_t));
                
                if (recv_calculated_crc32 == recv_ota_info->crc32 && 
                    recv_ota_info->file_numbers == ota_file_info.file_numbers) {
                    ota_current_state = OTA_STATE_FILE_DATA_SENDING;
                    ota_file_index = 0;
                    ota_bytes_sent = 0;
                    hal_sysfreq_req(HAL_SYSFREQ_USER_APP_0, HAL_CMU_FREQ_208M);
                    TRACE(0, "spictrl: OTA Protocol: Step 6 - File info verified, starting data transfer");
                    if (ota_state_timer_id) {
                        osTimerStart(ota_state_timer_id, 10);
                    }                    
                } else {
                    TRACE(0, "spictrl: OTA Protocol: File info verification failed");
                    ret = -1;
                }
            }
            break;
            
        case OTA_STATE_FILE_DATA_SENDING:
            // 处理文件数据发送
            if (ota_file_index < ota_file_info.file_numbers) {
                uint32_t file_length = ota_file_info.file_length[ota_file_index];
                const char *filename = (const char*)ota_file_info.file_name[ota_file_index];
                
                if (ota_bytes_sent < file_length) {
                    static struct OTA_FILE_DATA_T ota_file_data;
                    
                    int data_len = read_ota_file_data(filename, ota_bytes_sent, &ota_file_data);
                    if (data_len > 0) {
                        uint32_t remaining = file_length - ota_bytes_sent;
                        if (data_len > remaining) {
                            data_len = remaining;
                        }
                        
                        memset(spi_buffer, 0, sizeof(spi_buffer));
                        memcpy(spi_buffer, &ota_file_data, sizeof(struct OTA_FILE_DATA_T));
                        
                        ret = hal_user_spi_send(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                        if (ret == 0) {
                            // 启动定时器等待接收响应
                            if (ota_state_timer_id) {
                                osTimerStart(ota_state_timer_id, 100);
                            }
                            // 触发数据发送
                            if (gpio_spi_event_flags) {
                                osEventFlagsSet(gpio_spi_event_flags, SPI_EVENT_TX_COMPLETE);
                            }
                            TRACE(0, "spictrl: OTA Protocol: Sent file data, offset=%d, length=%d", 
                                  ota_bytes_sent, data_len);
                        }
                    } else {
                        ret = -1;
                    }
                } else {
                    // 当前文件传输完成，移到下一个文件
                    ota_file_index++;
                    ota_bytes_sent = 0;
                    TRACE(0, "spictrl: OTA Protocol: File %d transfer completed", ota_file_index - 1);
                    
                    if (ota_file_index >= ota_file_info.file_numbers) {
                        ota_current_state = OTA_STATE_TRANSFER_COMPLETE;
                        TRACE(0, "spictrl: OTA Protocol: All files transferred successfully!");
                        close_ota_package_file();
                        hal_sysfreq_req(HAL_SYSFREQ_USER_APP_0, HAL_CMU_FREQ_32K);
                    } else {
                        // 继续发送下一个文件
                        if (gpio_spi_event_flags) {
                            osEventFlagsSet(gpio_spi_event_flags, SPI_EVENT_TX_COMPLETE);
                        }
                    }
                }
            }
            break;
            
        case OTA_STATE_TRANSFER_COMPLETE:
            // 传输完成，重置状态
            ota_current_state = OTA_STATE_IDLE;
            ret = 0;
            break;
            
        default:
            ota_current_state = OTA_STATE_IDLE;
            ret = -1;
            break;
    }
    
    return ret;
}

// 处理接收到的数据确认
static int handle_ota_data_ack(void)
{
    static uint8_t spi_buffer[SPICOMM_LINKLAYER_DATA_SIZE];
    
    if (ota_current_state == OTA_STATE_FILE_DATA_SENDING) {
        int ret = hal_user_spi_recv_only(HAL_SPI_ID_0, spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
        if (ret == 0) {
            struct OTA_FILE_DATA_T *recv_data = (struct OTA_FILE_DATA_T*)spi_buffer;
            uint32_t recv_calculated_crc32 = calculate_crc32((uint8_t*)recv_data->file_data, 
                                                             SPICOMM_LINKLAYER_DATA_SIZE-4);
            
            if (recv_calculated_crc32 == recv_data->crc32) {
                // 数据确认成功，更新发送偏移
                uint32_t file_length = ota_file_info.file_length[ota_file_index];
                uint32_t remaining = file_length - ota_bytes_sent;
                uint32_t data_len = (remaining > (SPICOMM_LINKLAYER_DATA_SIZE-4)) ? 
                                   (SPICOMM_LINKLAYER_DATA_SIZE-4) : remaining;
                
                ota_bytes_sent += data_len;
                
                uint32_t progress = (ota_bytes_sent * 100) / file_length;
                TRACE(0, "spictrl: OTA Protocol: File data confirmed, progress: %d%% (%d/%d)", 
                      progress, ota_bytes_sent, file_length);
                
                // 继续发送下一块数据
                if (gpio_spi_event_flags) {
                    osEventFlagsSet(gpio_spi_event_flags, SPI_EVENT_TX_COMPLETE);
                }
                return 0;
            } else {
                TRACE(0, "spictrl: OTA Protocol: Data CRC verification failed, retransmission needed");
                // 重传当前数据块
                if (gpio_spi_event_flags) {
                    osEventFlagsSet(gpio_spi_event_flags, SPI_EVENT_TX_COMPLETE);
                }
                return -1;
            }
        }
    }
    return -1;
}

// 固件传送专用的GPIO自动处理线程
static void gpio_ota_firmware_auto_thread(void *arg)
{
    int ret;
    bool spi_initialized = false;
    
    // 创建状态定时器
    ota_state_timer_id = osTimerNew(ota_state_timer_callback, osTimerOnce, NULL, NULL);
    
    TRACE(0, "spictrl: OTA Firmware Transfer: Monitoring thread started, waiting for GPIO06 trigger...");
    
    while (1) {
        // 等待GPIO中断事件或其他事件
        uint32_t flags = osEventFlagsWait(gpio_spi_event_flags, 
                                         GPIO_SPI_TRIGGER_EVENT | GPIO_SPI_STOP_EVENT | 
                                         GPIO_SPI_UBOOT_DET_EVENT | SPI_EVENT_TIMEOUT |
                                         SPI_EVENT_TX_COMPLETE | SPI_EVENT_RX_COMPLETE,
                                         osFlagsWaitAny, osWaitForever);
        
        if (flags & GPIO_SPI_STOP_EVENT) {
            TRACE(0, "spictrl: OTA Firmware Transfer: Monitoring thread stopped");
            break;
        }
        
        if (flags & (GPIO_SPI_UBOOT_DET_EVENT | GPIO_SPI_TRIGGER_EVENT)) {
            if (!gpio_auto_enabled) {
                continue;
            }
            
            // 初始化SPI（如果还未初始化）
            if (!spi_initialized) {
                struct HAL_SPI_CFG_T spi_cfg = {
                    .rate = 10000000,        // 10MHz
                    .clk_delay_half = true,
                    .clk_polarity = true,
                    .slave = false,
                    .dma_rx = false,
                    .dma_tx = false,
                    .cs = 0,
                    .tx_bits = 8,
                    .rx_bits = 8,
                    .samp_delay = 0,
                };
                
                hal_iomux_set_spi();
                ret = hal_user_spi_open(HAL_SPI_ID_0, &spi_cfg);
                if (ret == 0) {
                    spi_initialized = true;
                    TRACE(0, "spictrl: OTA Firmware Transfer: SPI initialized successfully");
                }
            }
            
            if (spi_initialized) {
                if (flags & GPIO_SPI_UBOOT_DET_EVENT) {
                    // GPIO06高电平：OTA固件传送事件
                    TRACE(0, "spictrl: OTA Firmware Transfer: Starting firmware transfer...");
                    // 使用统一的OTA传输入口
                    ret = execute_ota_firmware_transfer();
                    if (ret != 0) {
                        TRACE(0, "spictrl: OTA Firmware Transfer: Transfer failed, ret=%d", ret);
                    }
                } else {
                    // GPIO06低电平：普通业务协议
                    ret = execute_auto_business_protocol();
                    if (ret != 0) {
                        TRACE(0, "spictrl: GPIO Auto SPI: Auto business protocol execution failed, ret=%d", ret);
                    }
                }
            }
        }
        
        // 处理状态机事件（仅在状态机模式下）
        if ((flags & (SPI_EVENT_TIMEOUT | SPI_EVENT_TX_COMPLETE)) && 
            get_ota_transfer_mode() == OTA_TRANSFER_MODE_STATE_MACHINE) {
            if (ota_current_state != OTA_STATE_IDLE && ota_current_state != OTA_STATE_TRANSFER_COMPLETE) {
                if (flags & SPI_EVENT_TIMEOUT) {
                    // 处理超时事件
                    ret = handle_ota_state_machine();
                    if (ret != 0) {
                        ota_retry_count++;
                        if (ota_retry_count > 3) {
                            TRACE(0, "spictrl: OTA Protocol: Max retries exceeded, aborting");
                            ota_current_state = OTA_STATE_IDLE;
                        } else {
                            TRACE(0, "spictrl: OTA Protocol: Retrying, attempt %d", ota_retry_count);
                            // 重试当前状态
                            handle_ota_state_machine();
                        }
                    } else {
                        ota_retry_count = 0;
                    }
                } else if (flags & SPI_EVENT_TX_COMPLETE) {
                    // 处理发送完成事件
                    if (ota_current_state == OTA_STATE_FILE_DATA_SENDING) {
                        handle_ota_data_ack();
                    } else {
                        handle_ota_state_machine();
                    }
                }
            }
        }
    }
    
    // 线程退出前清理
    if (ota_state_timer_id) {
        osTimerDelete(ota_state_timer_id);
        ota_state_timer_id = NULL;
    }
    osThreadExit();
}

static void spictrl_usage(eshell_session_t s)
{
    eshell_session_write(s, spictrl_helper, strlen(spictrl_helper));
}

// 显示控制帧结构信息
static void display_frame_info(eshell_session_t s, const ctl_fr_t *frame, const char *direction)
{
    eshell_session_printf(s, "%s Frame Info:" ESHELL_NEW_LINE, direction);
    eshell_session_printf(s, "  Frame Control:" ESHELL_NEW_LINE);
    eshell_session_printf(s, "    Version: 0x%02X" ESHELL_NEW_LINE, frame->fr_ctrl.ver);
    eshell_session_printf(s, "    Type: 0x%02X" ESHELL_NEW_LINE, frame->fr_ctrl.type);
    eshell_session_printf(s, "    Retry Quit: 0x%02X" ESHELL_NEW_LINE, frame->fr_ctrl.retry_quit);
    eshell_session_printf(s, "    Reserved: 0x%02X" ESHELL_NEW_LINE, frame->fr_ctrl.reserved);
    eshell_session_printf(s, "  Sequence Control: 0x%08X" ESHELL_NEW_LINE, frame->seqctl);
    eshell_session_printf(s, "  Data Length: %d bytes" ESHELL_NEW_LINE, frame->length);
    eshell_session_printf(s, "  FCS: 0x%08X" ESHELL_NEW_LINE, frame->fcs);
    
    // 显示数据内容
    if (frame->length > 0 && frame->length <= CTRL_MODE_DATA_LEN * sizeof(uint32_t)) {
        eshell_session_printf(s, "  Data Content:" ESHELL_NEW_LINE);
        const uint8_t *data_bytes = (const uint8_t *)frame->data;
        size_t actual_len = (frame->length > CTRL_MODE_DATA_LEN * sizeof(uint32_t)) ? 
                           CTRL_MODE_DATA_LEN * sizeof(uint32_t) : frame->length;
        
        for (size_t i = 0; i < actual_len; i++) {
            if (i % 16 == 0) {
                eshell_session_printf(s, "    %04x: ", i);
            }
            eshell_session_printf(s, "%02x ", data_bytes[i]);
            
            if ((i + 1) % 16 == 0 || (i + 1) == actual_len) {
                // 补充空格对齐
                if ((i + 1) % 16 != 0 && (i + 1) == actual_len) {
                    for (size_t j = (i + 1) % 16; j < 16; j++) {
                        eshell_session_printf(s, "   ");
                    }
                }
                
                // 显示ASCII字符
                eshell_session_printf(s, " |");
                size_t line_start = (i / 16) * 16;
                size_t line_end = (i + 1 > actual_len) ? actual_len : i + 1;
                for (size_t j = line_start; j < line_end; j++) {
                    char c = (data_bytes[j] >= 32 && data_bytes[j] <= 126) ? data_bytes[j] : '.';
                    eshell_session_printf(s, "%c", c);
                }
                eshell_session_printf(s, "|" ESHELL_NEW_LINE);
            }
        }
    }
}

void spi_data_send(eshell_session_t s, const char *arg)
{
    ctl_fr_t send_frame = {0};
    uint8_t hex_data[CTRL_MODE_DATA_LEN * sizeof(uint32_t)]; // 最大支持data字段大小
    size_t data_len = 0;
    size_t arg_len = 0;
    size_t i = 0;
    char hex_byte[3] = {0};
    char *endptr;
    unsigned long hex_val;
    static uint32_t sequence_counter = 0;

    if (arg == NULL) {
        eshell_session_printf(s, "[Error]: hex data argument is NULL" ESHELL_NEW_LINE);
        return;
    }

    arg_len = strlen(arg);
    
    // 检查输入长度（必须是偶数，因为每个字节需要2个十六进制字符）
    if (arg_len % 2 != 0) {
        eshell_session_printf(s, "[Error]: hex string length must be even" ESHELL_NEW_LINE);
        return;
    }

    data_len = arg_len / 2;
    if (data_len > sizeof(hex_data)) {
        eshell_session_printf(s, "[Error]: hex data too long (max %d bytes)" ESHELL_NEW_LINE, sizeof(hex_data));
        return;
    }

    // 解析十六进制字符串
    for (i = 0; i < data_len; i++) {
        // 提取两个字符作为一个字节
        hex_byte[0] = arg[i * 2];
        hex_byte[1] = arg[i * 2 + 1];
        hex_byte[2] = '\0';

        // 检查字符是否为有效的十六进制字符
        if (!((hex_byte[0] >= '0' && hex_byte[0] <= '9') ||
              (hex_byte[0] >= 'A' && hex_byte[0] <= 'F') ||
              (hex_byte[0] >= 'a' && hex_byte[0] <= 'f')) ||
            !((hex_byte[1] >= '0' && hex_byte[1] <= '9') ||
              (hex_byte[1] >= 'A' && hex_byte[1] <= 'F') ||
              (hex_byte[1] >= 'a' && hex_byte[1] <= 'f'))) {
            eshell_session_printf(s, "[Error]: invalid hex character at position %d-%d: '%c%c'" ESHELL_NEW_LINE, 
                                 i * 2, i * 2 + 1, hex_byte[0], hex_byte[1]);
            return;
        }

        // 转换为十六进制数值
        hex_val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0') {
            eshell_session_printf(s, "[Error]: failed to parse hex byte: %s" ESHELL_NEW_LINE, hex_byte);
            return;
        }

        hex_data[i] = (uint8_t)hex_val;
    }

    // 构建控制帧
    send_frame.fr_ctrl.ver = 0x01;          // 版本号
    send_frame.fr_ctrl.type = 0x00;         // 数据帧类型
    send_frame.fr_ctrl.retry_quit = 0x00;   // 重试退出标志
    send_frame.fr_ctrl.reserved = 0x00;     // 保留字段
    send_frame.seqctl = ++sequence_counter; // 序列号递增
    send_frame.length = data_len;           // 实际数据长度
    
    // 清空data字段并复制用户数据
    memset(send_frame.data, 0, sizeof(send_frame.data));
    memcpy(send_frame.data, hex_data, data_len);
    
    // 计算并设置FCS
    send_frame.fcs = calculate_fcs(&send_frame);

    // 显示要发送的帧信息
    display_frame_info(s, &send_frame, "Sending");

    // 配置SPI
    struct HAL_SPI_CFG_T spi_cfg = {
        .rate = 10000000,        // 10MHz
        .clk_delay_half = true,
        .clk_polarity = true,
        .slave = false,
        .dma_rx = false,
        .dma_tx = false,
        .cs = 0,
        .tx_bits = 8,
        .rx_bits = 8,
        .samp_delay = 0,
    };

    // 初始化SPI（如果还未初始化）
    static bool spi_initialized = false;
    if (!spi_initialized) {
        hal_iomux_set_spi();
        int ret = hal_user_spi_open(HAL_SPI_ID_0, &spi_cfg);
        if (ret) {
            eshell_session_printf(s, "SPI open failed: %d" ESHELL_NEW_LINE, ret);
            return;
        }
        spi_initialized = true;
        eshell_session_printf(s, "SPI initialized successfully." ESHELL_NEW_LINE);
    }

    // 发送控制帧 - 使用普通阻塞式API
    int ret = hal_user_spi_send(HAL_SPI_ID_0, (uint8_t*)&send_frame, sizeof(ctl_fr_t));
    if (ret) {
        eshell_session_printf(s, "SPI send failed: %d" ESHELL_NEW_LINE, ret);
    } else {
        eshell_session_printf(s, "SPI control frame sent successfully (total %d bytes)." ESHELL_NEW_LINE, sizeof(ctl_fr_t));
    }
}

void spi_data_recv(eshell_session_t s, const char *arg)
{
    ctl_fr_t recv_frame = {0};

    // recv命令不再需要长度参数，因为总是接收固定大小的控制帧
    (void)arg; // 忽略参数

    // 配置SPI（如果还未初始化）
    struct HAL_SPI_CFG_T spi_cfg = {
        .rate = 10000000,        // 10MHz
        .clk_delay_half = true,
        .clk_polarity = true,
        .slave = false,
        .dma_rx = false,
        .dma_tx = false,
        .cs = 0,
        .tx_bits = 8,
        .rx_bits = 8,
        .samp_delay = 0,
    };

    static bool spi_initialized = false;
    if (!spi_initialized) {
        hal_iomux_set_spi();
        int ret = hal_user_spi_open(HAL_SPI_ID_0, &spi_cfg);
        if (ret) {
            eshell_session_printf(s, "SPI open failed: %d" ESHELL_NEW_LINE, ret);
            return;
        }
        spi_initialized = true;
        eshell_session_printf(s, "SPI initialized successfully." ESHELL_NEW_LINE);
    }

    // 接收控制帧 - 使用普通阻塞式API
    int ret = hal_user_spi_recv_only(HAL_SPI_ID_0, (uint8_t*)&recv_frame, sizeof(ctl_fr_t));
    
    if (ret != 0) {
        eshell_session_printf(s, "SPI receive failed: %d" ESHELL_NEW_LINE, ret);
        return;
    }

    eshell_session_printf(s, "SPI control frame received successfully (total %d bytes)." ESHELL_NEW_LINE, sizeof(ctl_fr_t));

    // 显示接收到的帧信息
    display_frame_info(s, &recv_frame, "Received");

    // 验证FCS
    if (verify_fcs(&recv_frame)) {
        eshell_session_printf(s, "FCS verification: PASSED" ESHELL_NEW_LINE);
    } else {
        eshell_session_printf(s, "FCS verification: FAILED" ESHELL_NEW_LINE);
    }
}

void spi_data_transfer(eshell_session_t s, const char *send_arg, const char *recv_arg)
{
    ctl_fr_t send_frame = {0};
    ctl_fr_t recv_frame = {0};
    uint8_t hex_data[CTRL_MODE_DATA_LEN * sizeof(uint32_t)]; // 发送数据缓冲区
    size_t data_len = 0;
    size_t i = 0;
    char hex_byte[3] = {0};
    char *endptr;
    unsigned long hex_val;
    static uint32_t sequence_counter = 1000; // transfer命令使用不同的序列号起始值

    if (send_arg == NULL) {
        eshell_session_printf(s, "[Error]: send_data argument is NULL" ESHELL_NEW_LINE);
        return;
    }

    // recv_arg在transfer命令中被忽略，因为总是接收固定大小的控制帧
    (void)recv_arg;

    // 解析发送数据（十六进制字符串）
    size_t send_arg_len = strlen(send_arg);
    if (send_arg_len % 2 != 0) {
        eshell_session_printf(s, "[Error]: send hex string length must be even" ESHELL_NEW_LINE);
        return;
    }

    data_len = send_arg_len / 2;
    if (data_len > sizeof(hex_data)) {
        eshell_session_printf(s, "[Error]: send data too long (max %d bytes)" ESHELL_NEW_LINE, sizeof(hex_data));
        return;
    }

    // 转换十六进制字符串为字节数组
    for (i = 0; i < data_len; i++) {
        hex_byte[0] = send_arg[i * 2];
        hex_byte[1] = send_arg[i * 2 + 1];
        hex_byte[2] = '\0';

        hex_val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0') {
            eshell_session_printf(s, "[Error]: failed to parse hex byte: %s" ESHELL_NEW_LINE, hex_byte);
            return;
        }
        hex_data[i] = (uint8_t)hex_val;
    }

    // 构建发送控制帧
    send_frame.fr_ctrl.ver = 0x01;          // 版本号
    send_frame.fr_ctrl.type = 0x01;         // 请求帧类型
    send_frame.fr_ctrl.retry_quit = 0x00;   // 重试退出标志
    send_frame.fr_ctrl.reserved = 0x00;     // 保留字段
    send_frame.seqctl = ++sequence_counter; // 序列号递增
    send_frame.length = data_len;           // 实际数据长度
    
    // 清空data字段并复制用户数据
    memset(send_frame.data, 0, sizeof(send_frame.data));
    memcpy(send_frame.data, hex_data, data_len);
    
    // 计算并设置FCS
    send_frame.fcs = calculate_fcs(&send_frame);

    // 配置SPI（如果还未初始化）
    struct HAL_SPI_CFG_T spi_cfg = {
        .rate = 10000000,        // 10MHz
        .clk_delay_half = true,
        .clk_polarity = true,
        .slave = false,
        .dma_rx = false,
        .dma_tx = false,
        .cs = 0,
        .tx_bits = 8,
        .rx_bits = 8,
        .samp_delay = 0,
    };

    static bool spi_initialized = false;
    if (!spi_initialized) {
        hal_iomux_set_spi();
        int ret = hal_user_spi_open(HAL_SPI_ID_0, &spi_cfg);
        if (ret) {
            eshell_session_printf(s, "SPI open failed: %d" ESHELL_NEW_LINE, ret);
            return;
        }
        spi_initialized = true;
        eshell_session_printf(s, "SPI initialized successfully." ESHELL_NEW_LINE);
    }

    // 发送命令并接收响应 - 使用普通阻塞式API
    int ret = hal_user_spi_recv(HAL_SPI_ID_0, (uint8_t*)&send_frame, (uint8_t*)&recv_frame, sizeof(ctl_fr_t));
    if (ret) {
        eshell_session_printf(s, "SPI transfer failed: %d" ESHELL_NEW_LINE, ret);
        return;
    }

    eshell_session_printf(s, "SPI control frame transfer completed successfully." ESHELL_NEW_LINE);

    // 显示发送的帧信息
    display_frame_info(s, &send_frame, "Sent");

    // 显示接收的帧信息
    display_frame_info(s, &recv_frame, "Received");

    // 验证接收帧的FCS
    if (verify_fcs(&recv_frame)) {
        eshell_session_printf(s, "Received frame FCS verification: PASSED" ESHELL_NEW_LINE);
    } else {
        eshell_session_printf(s, "Received frame FCS verification: FAILED" ESHELL_NEW_LINE);
    }
}

// GPIO自动SPI控制功能
static int gpio_auto_spi_start(enum HAL_GPIO_PIN_T gpio_pin)
{
    osThreadAttr_t thread_attr = {0};
    struct HAL_GPIO_IRQ_CFG_T gpio_cfg = {0};
    
    if (gpio_spi_thread_id != NULL || gpio_spi_event_flags != NULL) {
        return -1; // 已经启动
    }
    
    // 创建事件标志
    gpio_spi_event_flags = osEventFlagsNew(NULL);
    if (gpio_spi_event_flags == NULL) {
        return -2; // 创建事件失败
    }
    
    // 创建处理线程
    thread_attr.name = "gpio_spi_auto";
    thread_attr.stack_size = 8192; // 增加栈空间从2048到8192字节
    thread_attr.priority = osPriorityNormal;
    
    gpio_spi_thread_id = osThreadNew(gpio_ota_firmware_auto_thread, NULL, &thread_attr);
    if (gpio_spi_thread_id == NULL) {
        osEventFlagsDelete(gpio_spi_event_flags);
        gpio_spi_event_flags = NULL;
        return -3; // 创建线程失败
    }
    
    // 初始化GPIO功能
    // 配置GPIO为输入模式，启用内部上拉电阻
    struct HAL_IOMUX_PIN_FUNCTION_MAP pin_func_map = {
        .pin = gpio_pin,
        .function = HAL_IOMUX_FUNC_AS_GPIO,
        .volt = HAL_IOMUX_PIN_VOLTAGE_VIO,
        .pull_sel = HAL_IOMUX_PIN_PULLUP_ENABLE,  // 启用上拉电阻
    };
    hal_iomux_init(&pin_func_map, 1);
    
    // 设置GPIO方向为输入
    hal_gpio_pin_set_dir(gpio_pin, HAL_GPIO_DIR_IN, 0);
    
    // 配置GPIO中断
    gpio_cfg.irq_enable = true;
    gpio_cfg.irq_debounce = true;
    gpio_cfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
    gpio_cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING; // 上升沿触发
    gpio_cfg.irq_handler = gpio_spi_irq_handler;
    
    int ret = hal_gpio_setup_irq(gpio_pin, &gpio_cfg);
    if (ret != 0) {
        // GPIO配置失败，清理资源
        osEventFlagsSet(gpio_spi_event_flags, GPIO_SPI_STOP_EVENT);
        osThreadTerminate(gpio_spi_thread_id);
        osEventFlagsDelete(gpio_spi_event_flags);
        gpio_spi_thread_id = NULL;
        gpio_spi_event_flags = NULL;
        return -4; // GPIO配置失败
    }
    
    auto_gpio_pin = gpio_pin;
    gpio_auto_enabled = true;
    
    return 0; // 成功
}

static int gpio_auto_spi_stop(void)
{
    if (gpio_spi_event_flags == NULL && gpio_spi_thread_id == NULL) {
        return -1; // 未启动
    }
    
    gpio_auto_enabled = false;
    
    // 禁用GPIO中断
    if (auto_gpio_pin != -1) {
        struct HAL_GPIO_IRQ_CFG_T gpio_cfg = {0};
        gpio_cfg.irq_enable = false;
        hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)auto_gpio_pin, &gpio_cfg);
        auto_gpio_pin = -1;
    }
    
    // 发送停止信号给线程
    if (gpio_spi_event_flags != NULL) {
        osEventFlagsSet(gpio_spi_event_flags, GPIO_SPI_STOP_EVENT);
    }
    
    // 等待线程结束
    if (gpio_spi_thread_id != NULL) {
        osThreadTerminate(gpio_spi_thread_id);
        gpio_spi_thread_id = NULL;
    }
    
    // 删除事件标志
    if (gpio_spi_event_flags != NULL) {
        osEventFlagsDelete(gpio_spi_event_flags);
        gpio_spi_event_flags = NULL;
    }
    
    return 0;
}

static void spi_gpio_auto_command(eshell_session_t s, const char *gpio_arg, const char *enable_arg)
{
    if (gpio_arg == NULL || enable_arg == NULL) {
        eshell_session_printf(s, "[Error]: gpio_auto requires gpio_pin and enable arguments" ESHELL_NEW_LINE);
        return;
    }
    
    // 解析GPIO引脚号
    int gpio_pin_num = atoi(gpio_arg);
    if (gpio_pin_num < 0 || gpio_pin_num >= 96) { // Best1700有约96个GPIO引脚
        eshell_session_printf(s, "[Error]: Invalid GPIO pin number: %d" ESHELL_NEW_LINE, gpio_pin_num);
        return;
    }
    enum HAL_GPIO_PIN_T gpio_pin = (enum HAL_GPIO_PIN_T)gpio_pin_num;
    
    // 解析使能状态
    int enable = atoi(enable_arg);
    
    if (enable) {
        // 启动GPIO自动SPI模式
        int ret = gpio_auto_spi_start(gpio_pin);
        if (ret == 0) {
            // 同时初始化GPIO06作为输入检测引脚
            struct HAL_IOMUX_PIN_FUNCTION_MAP gpio06_pin_func = {
                .pin = HAL_GPIO_PIN_P0_6,
                .function = HAL_IOMUX_FUNC_AS_GPIO,
                .volt = HAL_IOMUX_PIN_VOLTAGE_VIO,
                .pull_sel = HAL_IOMUX_PIN_PULLUP_ENABLE,
            };
            hal_iomux_init(&gpio06_pin_func, 1);
            hal_gpio_pin_set_dir(HAL_GPIO_PIN_P0_6, HAL_GPIO_DIR_IN, 0);
            
            eshell_session_printf(s, "[Success]: GPIO auto SPI enabled on pin %d" ESHELL_NEW_LINE, gpio_pin_num);
            eshell_session_printf(s, "[Info]: GPIO06 initialized as input detection pin" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Error]: Failed to enable GPIO auto SPI, error: %d" ESHELL_NEW_LINE, ret);
        }
    } else {
        // 停止GPIO自动SPI模式
        int ret = gpio_auto_spi_stop();
        if (ret == 0) {
            eshell_session_printf(s, "[Success]: GPIO auto SPI disabled" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Error]: Failed to disable GPIO auto SPI, error: %d" ESHELL_NEW_LINE, ret);
        }
    }
}

static void spictrl_cmd(eshell_session_t s, int argc, char *argv[])
{
    char *action = NULL;

    if (argc < 2) {
        spictrl_usage(s);
        return;
    }

    action = argv[1];

    if ((memcmp("send", action, 4)) == 0) {
        spi_data_send(s, argv[2]);
    } else if ((memcmp("recv", action, 4)) == 0) {
        spi_data_recv(s, argv[2]);
    } else if ((memcmp("transfer", action, 8)) == 0) {
        if (argc < 3) {
            eshell_session_printf(s, "[Error]: transfer requires send_data" ESHELL_NEW_LINE);
            spictrl_usage(s);
            return;
        }
        spi_data_transfer(s, argv[2], NULL);
    } else if ((memcmp("help", action, 4)) == 0) {
        spictrl_usage(s);
    } else if ((memcmp("gpio_auto", action, 8)) == 0) {
        if (argc < 4) {
            eshell_session_printf(s, "[Error]: gpio_auto requires gpio_pin and enable" ESHELL_NEW_LINE);
            spictrl_usage(s);
            return;
        }
        spi_gpio_auto_command(s, argv[2], argv[3]);
    } else if ((memcmp("photo", action, 5)) == 0) {
        eshell_session_printf(s, "[Info]: Executing photo protocol manually..." ESHELL_NEW_LINE);
        int ret = execute_photo_protocol();
        if (ret == 0) {
            eshell_session_printf(s, "[Success]: Photo protocol completed successfully" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Error]: Photo protocol failed, error: %d" ESHELL_NEW_LINE, ret);
        }
    } else if ((memcmp("video", action, 5)) == 0) {
        eshell_session_printf(s, "[Info]: Executing video protocol manually..." ESHELL_NEW_LINE);
        int ret = execute_video_protocol();
        if (ret == 0) {
            eshell_session_printf(s, "[Success]: Video protocol completed successfully" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Error]: Video protocol failed, error: %d" ESHELL_NEW_LINE, ret);
        }
    } else if ((memcmp("ota_start", action, 9)) == 0) {
        eshell_session_printf(s, "[Info]: Starting OTA firmware transfer monitoring..." ESHELL_NEW_LINE);
        eshell_session_printf(s, "[Info]: Please specify trigger GPIO pin with: spictrl gpio_auto <pin> 1" ESHELL_NEW_LINE);
        eshell_session_printf(s, "[Info]: GPIO06 will be automatically initialized as detection pin" ESHELL_NEW_LINE);
        eshell_session_printf(s, "[Example]: spictrl gpio_auto 12 1  # Use GPIO12 as trigger, GPIO06 as detection" ESHELL_NEW_LINE);
    } else if ((memcmp("ota_stop", action, 8)) == 0) {
        eshell_session_printf(s, "[Info]: Stopping OTA firmware transfer monitoring..." ESHELL_NEW_LINE);
        int ret = gpio_auto_spi_stop();
        if (ret == 0) {
            eshell_session_printf(s, "[Success]: OTA monitoring stopped" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Error]: Failed to stop OTA monitoring, error: %d" ESHELL_NEW_LINE, ret);
        }
    } else if ((memcmp("ota_test", action, 8)) == 0) {
        eshell_session_printf(s, "[Info]: Testing OTA package file reading..." ESHELL_NEW_LINE);
        
        // 测试文件信息读取
        struct OTA_FILE_INFO_T test_info;
        int ret = load_ota_file_info(&test_info);
        if (ret != 0) {
            eshell_session_printf(s, "[Error]: Failed to load OTA file info, error: %d" ESHELL_NEW_LINE, ret);
            return;
        }
        
        eshell_session_printf(s, "[Success]: OTA file info loaded successfully" ESHELL_NEW_LINE);
        eshell_session_printf(s, "  Package: %s" ESHELL_NEW_LINE, OTA_PACKAGE_PATH);
        eshell_session_printf(s, "  File numbers: %d" ESHELL_NEW_LINE, test_info.file_numbers);
        eshell_session_printf(s, "  CRC32: 0x%08X" ESHELL_NEW_LINE, test_info.crc32);
        
        for (uint32_t i = 0; i < test_info.file_numbers && i < 3; i++) {
            eshell_session_printf(s, "  File[%d]: %s, type=%d, size=%d bytes" ESHELL_NEW_LINE, 
                                 i, test_info.file_name[i], test_info.file_type[i], test_info.file_length[i]);
        }
        
        // 测试第一个文件的数据读取
        if (test_info.file_numbers > 0) {
            struct OTA_FILE_DATA_T test_data;
            int data_len = read_ota_file_data((char*)test_info.file_name[0], 0, &test_data);
            if (data_len > 0) {
                eshell_session_printf(s, "[Success]: Read %d bytes from first file" ESHELL_NEW_LINE, data_len);
                eshell_session_printf(s, "  Data CRC32: 0x%08X" ESHELL_NEW_LINE, test_data.crc32);
            } else {
                eshell_session_printf(s, "[Error]: Failed to read file data, error: %d" ESHELL_NEW_LINE, data_len);
            }
        }
        
        // 关闭文件
        close_ota_package_file();
        eshell_session_printf(s, "[Info]: OTA package test completed" ESHELL_NEW_LINE);
    } else if ((memcmp("ota_mode", action, 8)) == 0) {
        if (argc < 3) {
            // 显示当前模式
            ota_transfer_mode_t current_mode = get_ota_transfer_mode();
            eshell_session_printf(s, "[Info]: Current OTA transfer mode: %s" ESHELL_NEW_LINE,
                                (current_mode == OTA_TRANSFER_MODE_BLOCKING) ? "BLOCKING" : "STATE_MACHINE");
            eshell_session_printf(s, "[Usage]: spictrl ota_mode <mode>" ESHELL_NEW_LINE);
            eshell_session_printf(s, "  <mode>: 0 = BLOCKING (阻塞模式), 1 = STATE_MACHINE (状态机模式)" ESHELL_NEW_LINE);
            return;
        }
        
        // 设置新模式
        int mode_num = atoi(argv[2]);
        if (mode_num < 0 || mode_num > 1) {
            eshell_session_printf(s, "[Error]: Invalid mode number: %d (should be 0 or 1)" ESHELL_NEW_LINE, mode_num);
            return;
        }
        
        ota_transfer_mode_t new_mode = (ota_transfer_mode_t)mode_num;
        set_ota_transfer_mode(new_mode);
        eshell_session_printf(s, "[Success]: OTA transfer mode set to %s" ESHELL_NEW_LINE,
                            (new_mode == OTA_TRANSFER_MODE_BLOCKING) ? "BLOCKING" : "STATE_MACHINE");
        
        if (new_mode == OTA_TRANSFER_MODE_BLOCKING) {
            eshell_session_printf(s, "[Info]: BLOCKING mode uses osDelay() for synchronous waiting" ESHELL_NEW_LINE);
        } else {
            eshell_session_printf(s, "[Info]: STATE_MACHINE mode uses timers and event-driven processing" ESHELL_NEW_LINE);
        }
    } else {
        eshell_session_printf(s, "[Error]: unknown option" ESHELL_NEW_LINE);
        spictrl_usage(s);
    }
}

ESHELL_DEF_BUILTIN_COMMAND("spictrl", spictrl_helper, spictrl_cmd);

#endif