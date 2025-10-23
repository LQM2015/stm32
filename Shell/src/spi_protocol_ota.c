/**
 * @file spi_protocol_ota.c
 * @brief SPI Protocol for OTA Implementation
 * @version 1.0
 * @date 2025-01-22
 * 
 * @note This is a framework file. The complete implementation with all OTA logic
 *       should be adapted from the original shell_spictrl.c file.
 */

#include "spi_protocol_ota.h"
#include "cmsis_os2.h"
#include <string.h>

/* =================================================================== */
/* Private Types                                                      */
/* =================================================================== */

/* Message structure for events (same as dispatcher) */
typedef struct {
    uint32_t event_type;
    uint32_t timestamp;
} gpio_event_msg_t;

/* =================================================================== */
/* Private Variables                                                  */
/* =================================================================== */

/* External message queue for event handling */
extern osMessageQueueId_t gpio_spi_event_queue;

static uint32_t g_ota_sequence_counter = 3000;
static ota_transfer_mode_t g_ota_transfer_mode = OTA_TRANSFER_MODE_STATE_MACHINE;

/* File information cache */
static OTA_FILE_INFO_T g_cached_ota_info = {0};
static bool g_ota_info_loaded = false;

/* File handle */
static platform_file_t g_ota_package_file;
static bool g_ota_file_opened = false;
static uint32_t g_ota_package_size = 0;

/* State machine variables */
static ota_protocol_state_t g_ota_current_state = OTA_STATE_IDLE;
static osTimerId_t g_ota_state_timer_id = NULL;
static uint32_t g_ota_retry_count = 0;
static uint32_t g_ota_file_index = 0;
static uint32_t g_ota_bytes_sent = 0;

/* =================================================================== */
/* OTA Configuration Functions                                        */
/* =================================================================== */

void spi_protocol_ota_set_mode(ota_transfer_mode_t mode)
{
    g_ota_transfer_mode = mode;
    TRACE_INFO("OTA transfer mode set to %s", 
               (mode == OTA_TRANSFER_MODE_BLOCKING) ? "BLOCKING" : "STATE_MACHINE");
}

ota_transfer_mode_t spi_protocol_ota_get_mode(void)
{
    return g_ota_transfer_mode;
}

/* =================================================================== */
/* OTA File Operations                                                */
/* =================================================================== */

/**
 * @brief Calculate file data offset in package
 */
static uint32_t calculate_file_data_offset(int file_index)
{
    uint32_t offset = sizeof(OTA_FILE_INFO_T);  // Skip file info header
    
    // Add sizes of all previous files
    for (int i = 0; i < file_index; i++) {
        offset += g_cached_ota_info.file_length[i];
    }
    
    return offset;
}

int spi_protocol_ota_load_file_info(OTA_FILE_INFO_T *ota_info)
{
    int ret;
    size_t bytes_read;
    uint32_t calculated_crc32;
    
    if (!ota_info) {
        return -1;
    }
    
    // Return cached info if already loaded
    if (g_ota_info_loaded) {
        memcpy(ota_info, &g_cached_ota_info, sizeof(OTA_FILE_INFO_T));
        TRACE_INFO("OTA: Using cached file info - %d files", ota_info->file_numbers);
        return 0;
    }
    
    // Get file size
    size_t file_size;
    ret = platform_file_stat(OTA_PACKAGE_PATH, &file_size);
    if (ret != 0) {
        TRACE_ERROR("OTA: Package file not found: %s", OTA_PACKAGE_PATH);
        return -2;
    }
    
    g_ota_package_size = file_size;
    TRACE_INFO("OTA: Package file size: %d bytes", g_ota_package_size);
    
    // Check minimum size
    if (g_ota_package_size < sizeof(OTA_FILE_INFO_T)) {
        TRACE_ERROR("OTA: Package file too small: %d < %d", 
                    g_ota_package_size, sizeof(OTA_FILE_INFO_T));
        return -3;
    }
    
    // Open file
    ret = platform_file_open(&g_ota_package_file, OTA_PACKAGE_PATH, FA_READ);
    if (ret != 0) {
        TRACE_ERROR("OTA: Failed to open package file");
        return -4;
    }
    g_ota_file_opened = true;
    
    // Read file info header
    ret = platform_file_read(&g_ota_package_file, &g_cached_ota_info, 
                             sizeof(OTA_FILE_INFO_T), &bytes_read);
    if (ret != 0 || bytes_read != sizeof(OTA_FILE_INFO_T)) {
        TRACE_ERROR("OTA: Failed to read file info");
        platform_file_close(&g_ota_package_file);
        g_ota_file_opened = false;
        return -5;
    }
    
    // Verify CRC32
    calculated_crc32 = spi_protocol_calculate_crc32((uint8_t*)&g_cached_ota_info, 
                                                     sizeof(OTA_FILE_INFO_T) - sizeof(uint32_t));
    if (calculated_crc32 != g_cached_ota_info.crc32) {
        TRACE_ERROR("OTA: File info CRC32 verification failed");
        TRACE_ERROR("OTA: Calculated=0x%08X, Expected=0x%08X", 
                    calculated_crc32, g_cached_ota_info.crc32);
        platform_file_close(&g_ota_package_file);
        g_ota_file_opened = false;
        return -6;
    }
    
    // Validate file numbers
    if (g_cached_ota_info.file_numbers == 0 || 
        g_cached_ota_info.file_numbers > LWK_OTA_BIN_MAX) {
        TRACE_ERROR("OTA: Invalid file numbers: %d", g_cached_ota_info.file_numbers);
        platform_file_close(&g_ota_package_file);
        g_ota_file_opened = false;
        return -7;
    }
    
    // Copy to output
    memcpy(ota_info, &g_cached_ota_info, sizeof(OTA_FILE_INFO_T));
    g_ota_info_loaded = true;
    
    // Print file information
    TRACE_INFO("OTA: Successfully loaded file info");
    TRACE_INFO("OTA: File numbers: %d, CRC32: 0x%08X", 
               ota_info->file_numbers, ota_info->crc32);
    
    for (uint32_t i = 0; i < ota_info->file_numbers; i++) {
        TRACE_INFO("OTA: File[%d]: %s, type=%d, addr=0x%08X, size=%d", 
                   i, ota_info->file_name[i], ota_info->file_type[i], 
                   ota_info->file_start_addr[i], ota_info->file_length[i]);
    }
    
    return 0;
}

int spi_protocol_ota_read_file_data(const char *filename, uint32_t offset, 
                                     OTA_FILE_DATA_T *ota_data)
{
    int file_index = -1;
    uint32_t file_data_offset;
    uint32_t read_size;
    size_t bytes_read;
    int ret;
    
    if (!filename || !ota_data) {
        return -1;
    }
    
    // Clear buffer
    memset(ota_data, 0, sizeof(OTA_FILE_DATA_T));
    
    // Ensure file info is loaded
    if (!g_ota_info_loaded) {
        TRACE_ERROR("OTA: File info not loaded");
        return -2;
    }
    
    // Ensure file is opened
    if (!g_ota_file_opened) {
        TRACE_ERROR("OTA: Package file not opened");
        return -3;
    }
    
    // Find file index by name
    for (uint32_t i = 0; i < g_cached_ota_info.file_numbers; i++) {
        if (strcmp((char*)g_cached_ota_info.file_name[i], filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    if (file_index < 0) {
        TRACE_ERROR("OTA: File not found: %s", filename);
        return -4;
    }
    
    // Check offset
    if (offset >= g_cached_ota_info.file_length[file_index]) {
        TRACE_ERROR("OTA: Offset exceeds file size");
        return -5;
    }
    
    // Calculate read size
    uint32_t remaining = g_cached_ota_info.file_length[file_index] - offset;
    read_size = (remaining > (SPICOMM_LINKLAYER_DATA_SIZE - 4)) ? 
                (SPICOMM_LINKLAYER_DATA_SIZE - 4) : remaining;
    
    // Calculate absolute offset in package
    file_data_offset = calculate_file_data_offset(file_index) + offset;
    
    // Seek to position
    ret = platform_file_seek(&g_ota_package_file, file_data_offset);
    if (ret != 0) {
        TRACE_ERROR("OTA: Failed to seek to offset %d", file_data_offset);
        return -6;
    }
    
    // Read data
    ret = platform_file_read(&g_ota_package_file, ota_data->file_data, 
                             read_size, &bytes_read);
    if (ret != 0 || bytes_read != read_size) {
        TRACE_ERROR("OTA: Failed to read file data");
        return -7;
    }
    
    // Calculate CRC32
    ota_data->crc32 = spi_protocol_calculate_crc32((uint8_t*)ota_data->file_data, read_size);
    
    TRACE_DEBUG("OTA: Read %d bytes from '%s' at offset %d, CRC32=0x%08X", 
                read_size, filename, offset, ota_data->crc32);
    
    return read_size;
}

bool spi_protocol_ota_check_package_exist(uint32_t ota_size, const char *ota_name)
{
    size_t file_size;
    int ret;
    
    if (!ota_name) {
        return false;
    }
    
    // Get file size
    ret = platform_file_stat(OTA_PACKAGE_PATH, &file_size);
    if (ret != 0) {
        TRACE_ERROR("OTA: Package file not found");
        return false;
    }
    
    // Extract filename from path
    const char *filename = strrchr(OTA_PACKAGE_PATH, '/');
    if (filename) {
        filename++; // Skip '/'
    } else {
        filename = OTA_PACKAGE_PATH;
    }
    
    // Compare
    bool name_match = (strcmp(ota_name, filename) == 0);
    bool size_match = (file_size == ota_size);
    
    TRACE_INFO("OTA: Package check - name=%s, size=%s", 
               name_match ? "MATCH" : "MISMATCH",
               size_match ? "MATCH" : "MISMATCH");
    
    return (name_match && size_match);
}

/* =================================================================== */
/* OTA Protocol Execution Functions                                   */
/* =================================================================== */

/**
 * @note The complete OTA protocol implementations should be adapted from
 *       the original shell_spictrl.c file. Below are function prototypes
 *       and basic structure.
 */

int spi_protocol_ota_upgrade_execute(void)
{
    ctl_fr_t send_frame, recv_frame;
    int ret;
    const char *protocol_name = "OTA";
    const int max_wait_cycles = 300; // 最多等待300个周期（300秒=5分钟）
    int wait_cycle = 0;
    
    TRACE_INFO("spictrl: %s Protocol: Starting OTA upgrade protocol from step 3...", protocol_name);
    
    // 步骤3-5: 循环发送空包并等待接收OTA请求
    while (wait_cycle < max_wait_cycles) 
    {
        // 发送空包 [0x00, 0x00]
        uint32_t empty_data[2] = {OTA_EMPTY_CMD, OTA_EMPTY_DATA};
        ret = spi_protocol_build_frame(&send_frame, SPI_CMD_NORMAL, empty_data, 
                                      2*sizeof(uint32_t), ++g_ota_sequence_counter);
        if (ret != 0) {
            TRACE_ERROR("spictrl: %s Protocol: Failed to build empty frame", protocol_name);
            return -6;
        }
        
        TRACE_INFO("spictrl: %s Protocol: Cycle %d - Sent empty frame [0x%02X, 0x%02X], waiting for OTA request...", 
                  protocol_name, wait_cycle + 1, OTA_EMPTY_CMD, OTA_EMPTY_DATA);
        
        // Wait for GPIO interrupt event via message queue
        gpio_event_msg_t msg;
        osStatus_t status = osMessageQueueGet(gpio_spi_event_queue, &msg, NULL, osWaitForever);
        
        if (status != osOK) {
            TRACE_ERROR("spictrl: %s Protocol: Failed to receive message, status=%d", protocol_name, status);
            continue;
        }
        
        if (msg.event_type == GPIO_SPI_TRIGGER_EVENT) 
        {
            wait_cycle = 300;
            // Check if auto trigger is still enabled (implementation-specific)
            // if (!gpio_auto_enabled) continue;
            
            platform_delay_ms(100);
            ret = spi_protocol_send_frame(&send_frame);
            if (ret != 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to send empty frame, ret=%d", protocol_name, ret);
                return -7;
            }   
            
            platform_delay_ms(100);
            // 尝试接收OTA请求
            ret = spi_protocol_receive_frame(&recv_frame);
            if (ret == 0) {
                // 成功接收到数据，检查是否是OTA请求
                if (recv_frame.length >= sizeof(ota_request_t) && 
                    spi_protocol_validate_frame(&recv_frame, OTA_REQUEST_CMD, sizeof(ota_request_t))) {
                    
                    ota_request_t *ota_request = (ota_request_t*)recv_frame.data;
                    
                    TRACE_INFO("spictrl: %s Protocol: Received OTA request [cmd=0x%02X, subcmd=0x%02X, size=%d, name=%s]", 
                              protocol_name, ota_request->cmd, ota_request->subcmd, 
                              ota_request->ota_size, ota_request->ota_name);
                    
                    if (ota_request->subcmd == OTA_REQUEST_PACKAGE) {
                        // 检查文件系统中是否存在对应的OTA包
                        // 确保字符串安全处理
                        char safe_ota_name[32];
                        memset(safe_ota_name, 0, sizeof(safe_ota_name));
                        strncpy(safe_ota_name, ota_request->ota_name, sizeof(safe_ota_name) - 1);
                        
                        bool package_exist = spi_protocol_ota_check_package_exist(ota_request->ota_size, safe_ota_name);
                        
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
                        
                        ret = spi_protocol_build_frame(&send_frame, SPI_CMD_NORMAL, response_buffer, 
                                                      sizeof(ota_response), ++g_ota_sequence_counter);
                        if (ret != 0) {
                            TRACE_ERROR("spictrl: %s Protocol: Failed to build OTA response frame", protocol_name);
                            return -8;
                        }
                        
                        platform_delay_ms(100);
                        ret = spi_protocol_send_frame(&send_frame);
                        if (ret != 0) {
                            TRACE_ERROR("spictrl: %s Protocol: Failed to send OTA response, ret=%d", protocol_name, ret);
                            return -9;
                        }
                        
                        TRACE_INFO("spictrl: %s Protocol: Sent OTA response [0x%02X, 0x%02X, 0x%02X, 0x%02X] - Package %s", 
                                  protocol_name, ota_response.cmd, ota_response.status, 
                                  ota_response.subcmd, ota_response.result,
                                  package_exist ? "EXISTS" : "NOT FOUND");
                        
                        if (!package_exist) {
                            TRACE_WARNING("spictrl: %s Protocol: OTA package not found, exiting upgrade process", protocol_name);
                            return -10;
                        }
                        
                        // OTA包存在，继续下一步流程
                    } else if (ota_request->subcmd == OTA_REQUEST_END) {
                        TRACE_INFO("spictrl: %s Protocol: Received end upgrade command, terminating OTA process", protocol_name);
                        return -11; // 接收到结束升级命令
                    }
                }
            }
            
            platform_delay_ms(100);
            ret = spi_protocol_receive_frame(&recv_frame);
            if (ret != 0) {
                TRACE_WARNING("spictrl: %s Protocol: Failed to receive second OTA business confirm, ret=%d", protocol_name, ret);
                return -17;
            }

            platform_delay_ms(100);
            
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
            
            ret = spi_protocol_build_frame(&send_frame, SPI_CMD_NORMAL, boot_buffer, 
                                          sizeof(boot_command), ++g_ota_sequence_counter);
            if (ret != 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to build OTA boot command frame", protocol_name);
                return -19;
            }
            
            ret = spi_protocol_send_frame(&send_frame);
            if (ret != 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to send OTA boot command, ret=%d", protocol_name, ret);
                return -20;
            }
            
            TRACE_INFO("spictrl: %s Protocol: Step 9 - Sent OTA boot command [0x%02X, 0x%02X, 0x%02X, 0x%02X] to bes2700", 
                      protocol_name, boot_command.cmd, boot_command.status, boot_command.subcmd, boot_command.result);
            
            TRACE_INFO("spictrl: %s Protocol: OTA upgrade protocol completed successfully! bes2700 should restart into OTA boot mode.", protocol_name);
            TRACE_INFO("spictrl: %s Protocol: bes2700 will now enter OTA boot state for actual package reception", protocol_name);
            
            return 0; // 成功完成OTA升级流程
        }
        
        platform_delay_ms(100);
        ret = spi_protocol_receive_frame(&recv_frame);
        if (ret != 0) {
            TRACE_WARNING("spictrl: %s Protocol: Failed to receive second OTA business confirm, ret=%d", protocol_name, ret);
            // 这里不直接返回错误，继续循环
        }
        
        // 没有接收到OTA请求，继续循环
        wait_cycle++;
        if (wait_cycle % 60 == 0) {  // 每分钟打印一次状态
            TRACE_INFO("spictrl: %s Protocol: Still waiting for OTA request... (%d/%d cycles)", 
                      protocol_name, wait_cycle, max_wait_cycles);
        }
        
        if (wait_cycle >= max_wait_cycles) {
            TRACE_WARNING("spictrl: %s Protocol: Timeout waiting for OTA request (5 minutes), exiting upgrade process", protocol_name);
            return -12; // 超时退出
        }
    }
    
    return 0; // 成功完成OTA升级流程
}

int spi_protocol_ota_firmware_transfer_execute(void)
{
    ctl_fr_t send_frame, recv_frame;
    int ret;
    const char *protocol_name = "OTA_FIRMWARE";
    static uint8_t spi_buffer[SPICOMM_LINKLAYER_DATA_SIZE];
    
    TRACE_INFO("spictrl: %s Protocol: Starting OTA firmware transfer protocol (BES2700 in OTA boot mode)...", protocol_name);
    
    // 步骤1: 发送OTA升级锁 [0x0A, 0x01, 0x03, 0x01]
    uint32_t upgrade_lock_data[4] = {0x0A, 0x01, 0x03, 0x01};
    ret = spi_protocol_build_frame(&send_frame, OTA_SPI_CMD, upgrade_lock_data, 
                                   4*sizeof(uint32_t), ++g_ota_sequence_counter);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to build upgrade lock frame", protocol_name);
        return -3;
    }
    
    ret = spi_protocol_send_frame(&send_frame);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to send upgrade lock, ret=%d", protocol_name, ret);
        return -4;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 1 - Sent OTA upgrade lock [0x0A, 0x01, 0x03, 0x01]", protocol_name);
    
    // 步骤2: 接收空包响应 [0x00, 0x00]
    platform_delay_ms(100);
    ret = spi_protocol_receive_frame(&recv_frame);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to receive empty response, ret=%d", protocol_name, ret);
        return -5;
    }
    
    if (!spi_protocol_validate_frame(&recv_frame, 0x00, 2)) {
        TRACE_ERROR("spictrl: %s Protocol: Invalid empty response received", protocol_name);
        return -6;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 2 - Received empty response [0x00, 0x00]", protocol_name);
    
    // 步骤3: 发送OTA包准备锁 [0x0A, 0x00, 0x00, 0x01]
    uint32_t package_lock_data[4] = {0x0A, 0x00, 0x00, 0x01};
    ret = spi_protocol_build_frame(&send_frame, OTA_SPI_CMD, package_lock_data, 
                                   4*sizeof(uint32_t), ++g_ota_sequence_counter);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to build package lock frame", protocol_name);
        return -7;
    }
    
    ret = spi_protocol_send_frame(&send_frame);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to send package lock, ret=%d", protocol_name, ret);
        return -8;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 3 - Sent OTA package lock [0x0A, 0x00, 0x00, 0x01]", protocol_name);
    
    // 步骤4: 接收OTA包请求 [0x09, 0x01]
    platform_delay_ms(100);
    ret = spi_protocol_receive_frame(&recv_frame);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to receive package request, ret=%d", protocol_name, ret);
        return -9;
    }
    
    if (!spi_protocol_validate_frame(&recv_frame, 0x09, 2)) {
        TRACE_ERROR("spictrl: %s Protocol: Invalid package request received", protocol_name);
        return -10;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 4 - Received OTA package request [0x09, 0x01]", protocol_name);
    
    // 步骤5: 通过spibuf[1028]发送OTA包头[struct OTA_FILE_INFO_T]
    OTA_FILE_INFO_T ota_file_info;
    
    ret = spi_protocol_ota_load_file_info(&ota_file_info);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to load OTA file info, ret=%d", protocol_name, ret);
        return -11;
    }
    
    // 将OTA文件信息拷贝到SPI缓冲区
    memset(spi_buffer, 0, sizeof(spi_buffer));
    memcpy(spi_buffer, &ota_file_info, sizeof(OTA_FILE_INFO_T));
    
    // 注意：spibuf不使用ctl_fr_t封包，直接发送原始数据
    ret = platform_spi_transmit(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to send OTA file info via spibuf, ret=%d", protocol_name, ret);
        return -12;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 5 - Sent OTA file info via spibuf[1028], %d files", 
              protocol_name, ota_file_info.file_numbers);
    
    // 步骤6: 通过spibuf[1028]接收OTA包头回复[struct OTA_FILE_INFO_T]
    platform_delay_ms(100);
    ret = platform_spi_receive(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
    if (ret != 0) {
        TRACE_ERROR("spictrl: %s Protocol: Failed to receive OTA file info reply via spibuf, ret=%d", protocol_name, ret);
        return -13;
    }
    
    // 校验返回的OTA文件信息
    OTA_FILE_INFO_T *recv_ota_info = (OTA_FILE_INFO_T*)spi_buffer;
    
    // 重新计算接收到的文件信息的CRC32
    uint32_t recv_calculated_crc32 = spi_protocol_calculate_crc32((uint8_t*)recv_ota_info, 
                                                                   sizeof(OTA_FILE_INFO_T) - sizeof(uint32_t));
    
    // 校验接收到的包的数据完整性
    if (recv_calculated_crc32 != recv_ota_info->crc32) {
        TRACE_ERROR("spictrl: %s Protocol: Received OTA file info CRC32 verification failed", protocol_name);
        TRACE_ERROR("spictrl: %s Protocol: Calculated CRC32=0x%08X, Received CRC32=0x%08X", 
                    protocol_name, recv_calculated_crc32, recv_ota_info->crc32);
        return -14;
    }
    
    // 校验文件数量是否匹配
    if (recv_ota_info->file_numbers != ota_file_info.file_numbers) {
        TRACE_ERROR("spictrl: %s Protocol: File numbers mismatch, expected=%d, received=%d", 
                    protocol_name, ota_file_info.file_numbers, recv_ota_info->file_numbers);
        return -15;
    }
    
    TRACE_INFO("spictrl: %s Protocol: Step 6 - Received OTA file info reply via spibuf[1028], verification passed", protocol_name);
    
    // 步骤7-8: 循环发送各个文件的数据包
    for (uint32_t file_idx = 0; file_idx < ota_file_info.file_numbers; file_idx++) {
        uint32_t file_length = ota_file_info.file_length[file_idx];
        uint32_t bytes_sent = 0;
        const char *filename = (const char*)ota_file_info.file_name[file_idx];
        
        TRACE_INFO("spictrl: %s Protocol: Starting transfer of file %d: '%s' (%d bytes)", 
                  protocol_name, file_idx, filename, file_length);
        
        while (bytes_sent < file_length) 
        {
            OTA_FILE_DATA_T ota_file_data;
            
            // 等待超时事件（如果使用定时器）
            if (g_ota_state_timer_id != NULL) {
                osTimerStart(g_ota_state_timer_id, 100);
            }
            
            // 可以添加事件等待逻辑
            // uint32_t flags = osEventFlagsWait(gpio_spi_event_flags, SPI_EVENT_TIMEOUT, osFlagsWaitAny, osWaitForever);
            
            // 读取文件数据
            int data_len = spi_protocol_ota_read_file_data(filename, bytes_sent, &ota_file_data);
            if (data_len <= 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to read file data, ret=%d", protocol_name, data_len);
                return -16;
            }
            
            // 调整数据长度，不超过剩余文件大小
            uint32_t remaining = file_length - bytes_sent;
            if (data_len > remaining) {
                data_len = remaining;
            }
            
            // 步骤7: 通过spibuf[1028]发送OTA数据包[struct OTA_FILE_DATA_T]
            memset(spi_buffer, 0, sizeof(spi_buffer));
            memcpy(spi_buffer, &ota_file_data, sizeof(OTA_FILE_DATA_T));
            
            TRACE_DEBUG("Sending data: %02x %02x %02x %02x...", 
                       spi_buffer[0], spi_buffer[1], spi_buffer[2], spi_buffer[3]);
            
            ret = platform_spi_transmit(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
            if (ret != 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to send file data via spibuf, ret=%d", protocol_name, ret);
                return -17;
            }
            
            TRACE_DEBUG("spictrl: %s Protocol: Step 7 - Sent file data via spibuf[1028], offset=%d, length=%d", 
                       protocol_name, bytes_sent, data_len);
            
            // 步骤8: 通过spibuf[1028]接收OTA数据包回复[struct OTA_FILE_DATA_T]
            ret = platform_spi_receive(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
            if (ret != 0) {
                TRACE_ERROR("spictrl: %s Protocol: Failed to receive file data reply via spibuf, ret=%d", protocol_name, ret);
                return -18;
            }
            
            // 校验返回的数据包
            OTA_FILE_DATA_T *recv_data = (OTA_FILE_DATA_T*)spi_buffer;
            
            // 重新计算接收到的文件数据的CRC32
            uint32_t recv_calculated_crc32 = spi_protocol_calculate_crc32((uint8_t*)recv_data->file_data, 
                                                                          SPICOMM_LINKLAYER_DATA_SIZE - 4);
            
            // 校验接收到的包的数据完整性
            TRACE_DEBUG("Received data: %02x %02x %02x %02x...", 
                       recv_data->file_data[0], recv_data->file_data[1], 
                       recv_data->file_data[2], recv_data->file_data[3]);
            
            if (recv_calculated_crc32 != recv_data->crc32) {
                TRACE_WARNING("spictrl: %s Protocol: Received file data CRC32 verification failed, retransmission needed", protocol_name);
                TRACE_WARNING("spictrl: %s Protocol: Calculated CRC32=0x%08X, Received CRC32=0x%08X", 
                             protocol_name, recv_calculated_crc32, recv_data->crc32);
                // 重发当前数据包
                continue;
            }
            
            // 进一步校验：对比发送和接收的数据是否一致
            if (memcmp(ota_file_data.file_data, recv_data->file_data, data_len) != 0) {
                TRACE_WARNING("spictrl: %s Protocol: File data content mismatch, retransmission needed", protocol_name);
                // 重发当前数据包
                continue;
            }
            
            TRACE_DEBUG("spictrl: %s Protocol: Step 8 - Received file data reply via spibuf[1028], verification passed", protocol_name);
            
            bytes_sent += data_len;
            
            // 显示传输进度
            uint32_t progress = (bytes_sent * 100) / file_length;
            if (bytes_sent % 10240 == 0 || bytes_sent == file_length) {
                TRACE_INFO("spictrl: %s Protocol: File '%s' transfer progress: %d%% (%d/%d bytes)", 
                          protocol_name, filename, progress, bytes_sent, file_length);
            }
        }
        
        TRACE_INFO("spictrl: %s Protocol: File %d '%s' transfer completed successfully", 
                  protocol_name, file_idx, filename);
    }
    
    TRACE_INFO("spictrl: %s Protocol: All OTA files transferred successfully!", protocol_name);
    TRACE_INFO("spictrl: %s Protocol: bes2700 should now process the received firmware packages", protocol_name);
    
    // 关闭OTA包文件（通过deinit自动处理）
    spi_protocol_ota_state_machine_deinit();
    
    return 0; // 成功完成OTA固件包传送流程
}

int spi_protocol_ota_transfer_execute(void)
{
    if (g_ota_transfer_mode == OTA_TRANSFER_MODE_BLOCKING) {
        return spi_protocol_ota_firmware_transfer_execute();
    } else {
        // State machine mode
        return spi_protocol_ota_state_machine_process();
    }
}

/* =================================================================== */
/* OTA State Machine Functions                                        */
/* =================================================================== */

/**
 * @brief Timer callback for state machine timeout
 */
static void ota_state_timer_callback(void *argument)
{
    (void)argument;
    // Send timeout event via message queue
    if (gpio_spi_event_queue != NULL) {
        gpio_event_msg_t msg;
        msg.event_type = SPI_EVENT_TIMEOUT;
        msg.timestamp = osKernelGetTickCount();
        osMessageQueuePut(gpio_spi_event_queue, &msg, 0, 0);
    }
}

int spi_protocol_ota_state_machine_init(void)
{
    TRACE_INFO("OTA: Initializing state machine...");
    
    // 创建状态定时器
    if (g_ota_state_timer_id == NULL) {
        g_ota_state_timer_id = osTimerNew((osTimerFunc_t)ota_state_timer_callback, 
                                          osTimerOnce, NULL, NULL);
        if (g_ota_state_timer_id == NULL) {
            TRACE_ERROR("OTA: Failed to create state timer");
            return -1;
        }
    }
    
    // 初始化状态变量
    g_ota_current_state = OTA_STATE_IDLE;
    g_ota_retry_count = 0;
    g_ota_file_index = 0;
    g_ota_bytes_sent = 0;
    
    TRACE_INFO("OTA: State machine initialized successfully");
    return 0;
}

int spi_protocol_ota_state_machine_process(void)
{
    ctl_fr_t send_frame, recv_frame;
    static uint8_t spi_buffer[SPICOMM_LINKLAYER_DATA_SIZE];
    int ret = 0;
    
    switch (g_ota_current_state) {
        case OTA_STATE_IDLE:
        {
            // 开始OTA升级锁发送
            uint32_t upgrade_lock_data[4] = {0x0A, 0x01, 0x03, 0x01};
            ret = spi_protocol_build_frame(&send_frame, OTA_SPI_CMD, upgrade_lock_data, 
                                          4*sizeof(uint32_t), ++g_ota_sequence_counter);
            if (ret == 0) {
                ret = spi_protocol_send_frame(&send_frame);
                if (ret == 0) {
                    g_ota_current_state = OTA_STATE_UPGRADE_LOCK_SENT;
                    // 启动超时定时器，100ms后检查
                    if (g_ota_state_timer_id) {
                        osTimerStart(g_ota_state_timer_id, 100);
                    }
                    TRACE_INFO("spictrl: OTA Protocol: Step 1 - Sent OTA upgrade lock");
                }
            }
        }
        break;
            
        case OTA_STATE_UPGRADE_LOCK_SENT:
            // 尝试接收空包响应
            ret = spi_protocol_receive_frame(&recv_frame);
            if (ret == 0) {
                if (spi_protocol_validate_frame(&recv_frame, 0x00, 2)) {
                    g_ota_current_state = OTA_STATE_EMPTY_RESPONSE_WAIT;
                    TRACE_INFO("spictrl: OTA Protocol: Step 2 - Received empty response");
                    // 继续下一步，发送包锁
                    uint32_t package_lock_data[4] = {0x0A, 0x00, 0x00, 0x01};
                    ret = spi_protocol_build_frame(&send_frame, OTA_SPI_CMD, package_lock_data, 
                                                   4*sizeof(uint32_t), ++g_ota_sequence_counter);
                    if (ret == 0) {
                        ret = spi_protocol_send_frame(&send_frame);
                        if (ret == 0) {
                            g_ota_current_state = OTA_STATE_PACKAGE_LOCK_SENT;
                            if (g_ota_state_timer_id) {
                                osTimerStart(g_ota_state_timer_id, 100);
                            }
                            TRACE_INFO("spictrl: OTA Protocol: Step 3 - Sent OTA package lock");
                        }
                    }
                } else {
                    TRACE_WARNING("spictrl: OTA Protocol: Invalid empty response received");
                    ret = -1;
                }
            }
            // 如果没有收到数据，等待下次调用
            break;
            
        case OTA_STATE_PACKAGE_LOCK_SENT:
            // 尝试接收OTA包请求
            ret = spi_protocol_receive_frame(&recv_frame);
            if (ret == 0) {
                if (spi_protocol_validate_frame(&recv_frame, 0x09, 2)) {
                    g_ota_current_state = OTA_STATE_PACKAGE_REQUEST_WAIT;
                    TRACE_INFO("spictrl: OTA Protocol: Step 4 - Received OTA package request");
                    
                    // 准备发送文件信息
                    OTA_FILE_INFO_T ota_info;
                    ret = spi_protocol_ota_load_file_info(&ota_info);
                    if (ret == 0) {
                        memset(spi_buffer, 0, sizeof(spi_buffer));
                        memcpy(spi_buffer, &ota_info, sizeof(OTA_FILE_INFO_T));
                        
                        ret = platform_spi_transmit(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                        if (ret == 0) {
                            g_ota_current_state = OTA_STATE_FILE_INFO_SENDING;
                            if (g_ota_state_timer_id) {
                                osTimerStart(g_ota_state_timer_id, 100);
                            }
                            TRACE_INFO("spictrl: OTA Protocol: Step 5 - Sent OTA file info");
                        }
                    }
                } else {
                    TRACE_WARNING("spictrl: OTA Protocol: Invalid package request received");
                    ret = -1;
                }
            }
            break;
            
        case OTA_STATE_FILE_INFO_SENDING:
            // 接收文件信息回复
            ret = platform_spi_receive(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
            if (ret == 0) {
                OTA_FILE_INFO_T *recv_ota_info = (OTA_FILE_INFO_T*)spi_buffer;
                uint32_t recv_calculated_crc32 = spi_protocol_calculate_crc32((uint8_t*)recv_ota_info, 
                                                                              sizeof(OTA_FILE_INFO_T) - sizeof(uint32_t));
                
                if (recv_calculated_crc32 == recv_ota_info->crc32 && 
                    recv_ota_info->file_numbers == g_cached_ota_info.file_numbers) {
                    g_ota_current_state = OTA_STATE_FILE_DATA_SENDING;
                    g_ota_file_index = 0;
                    g_ota_bytes_sent = 0;
                    TRACE_INFO("spictrl: OTA Protocol: Step 6 - File info verified, starting data transfer");
                    if (g_ota_state_timer_id) {
                        osTimerStart(g_ota_state_timer_id, 100);
                    }
                } else {
                    TRACE_ERROR("spictrl: OTA Protocol: File info verification failed");
                    ret = -1;
                }
            }
            break;
            
        case OTA_STATE_FILE_DATA_SENDING:
            // 处理文件数据发送
            if (g_ota_file_index < g_cached_ota_info.file_numbers) {
                uint32_t file_length = g_cached_ota_info.file_length[g_ota_file_index];
                const char *filename = (const char*)g_cached_ota_info.file_name[g_ota_file_index];
                
                if (g_ota_bytes_sent < file_length) {
                    OTA_FILE_DATA_T ota_file_data;
                    
                    int data_len = spi_protocol_ota_read_file_data(filename, g_ota_bytes_sent, &ota_file_data);
                    if (data_len > 0) {
                        uint32_t remaining = file_length - g_ota_bytes_sent;
                        if (data_len > remaining) {
                            data_len = remaining;
                        }
                        
                        memset(spi_buffer, 0, sizeof(spi_buffer));
                        memcpy(spi_buffer, &ota_file_data, sizeof(OTA_FILE_DATA_T));
                        
                        ret = platform_spi_transmit(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                        if (ret == 0) {
                            // 启动定时器等待接收响应
                            if (g_ota_state_timer_id) {
                                osTimerStart(g_ota_state_timer_id, 100);
                            }
                            TRACE_DEBUG("spictrl: OTA Protocol: Sent file data, offset=%d, length=%d", 
                                       g_ota_bytes_sent, data_len);
                            
                            // 接收确认
                            platform_delay_ms(10);
                            ret = platform_spi_receive(spi_buffer, SPICOMM_LINKLAYER_DATA_SIZE);
                            if (ret == 0) {
                                OTA_FILE_DATA_T *recv_data = (OTA_FILE_DATA_T*)spi_buffer;
                                uint32_t recv_crc = spi_protocol_calculate_crc32((uint8_t*)recv_data->file_data, 
                                                                                 SPICOMM_LINKLAYER_DATA_SIZE - 4);
                                
                                if (recv_crc == recv_data->crc32) {
                                    g_ota_bytes_sent += data_len;
                                    uint32_t progress = (g_ota_bytes_sent * 100) / file_length;
                                    if (g_ota_bytes_sent % 10240 == 0 || g_ota_bytes_sent == file_length) {
                                        TRACE_INFO("spictrl: OTA Protocol: File data confirmed, progress: %d%% (%d/%d)", 
                                                  progress, g_ota_bytes_sent, file_length);
                                    }
                                } else {
                                    TRACE_WARNING("spictrl: OTA Protocol: Data CRC verification failed, retransmission needed");
                                    ret = -1;
                                }
                            }
                        }
                    } else {
                        ret = -1;
                    }
                } else {
                    // 当前文件传输完成，移到下一个文件
                    g_ota_file_index++;
                    g_ota_bytes_sent = 0;
                    TRACE_INFO("spictrl: OTA Protocol: File %d transfer completed", g_ota_file_index - 1);
                    
                    if (g_ota_file_index >= g_cached_ota_info.file_numbers) {
                        g_ota_current_state = OTA_STATE_TRANSFER_COMPLETE;
                        TRACE_INFO("spictrl: OTA Protocol: All files transferred successfully!");
                    }
                }
            }
            break;
            
        case OTA_STATE_TRANSFER_COMPLETE:
            // 传输完成，重置状态
            g_ota_current_state = OTA_STATE_IDLE;
            ret = 0;
            break;
            
        default:
            g_ota_current_state = OTA_STATE_IDLE;
            ret = -1;
            break;
    }
    
    return ret;
}

void spi_protocol_ota_state_machine_deinit(void)
{
    if (g_ota_state_timer_id) {
        osTimerDelete(g_ota_state_timer_id);
        g_ota_state_timer_id = NULL;
    }
    
    // Close file if opened
    if (g_ota_file_opened) {
        platform_file_close(&g_ota_package_file);
        g_ota_file_opened = false;
    }
    
    g_ota_current_state = OTA_STATE_IDLE;
}

ota_protocol_state_t spi_protocol_ota_get_state(void)
{
    return g_ota_current_state;
}
