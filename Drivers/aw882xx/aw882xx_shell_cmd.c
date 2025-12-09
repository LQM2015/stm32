/**
 * @file aw882xx_shell_cmd.c
 * @brief Shell commands for AW882XX SmartPA driver
 * @version 1.0.0
 * @date 2025-01-16
 */

#include "shell.h"
#include "stm32_aw882xx_adapter.h"
#include "aw882xx.h"
#include "shell_log.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Initialize AW882XX SmartPA
 */
static int shell_aw882xx_init(int argc, char *argv[])
{
    int ret = aw882xx_adapter_init();
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "AW882XX initialized successfully\r\n");
    } else {
        shellPrint(shellGetCurrent(), "AW882XX initialization failed: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_init, shell_aw882xx_init, Initialize AW882XX SmartPA);

/**
 * @brief Deinitialize AW882XX SmartPA
 */
static int shell_aw882xx_deinit(int argc, char *argv[])
{
    aw882xx_adapter_deinit();
    shellPrint(shellGetCurrent(), "AW882XX deinitialized\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_deinit, shell_aw882xx_deinit, Deinitialize AW882XX SmartPA);

/**
 * @brief Start AW882XX SmartPA
 */
static int shell_aw882xx_start(int argc, char *argv[])
{
    int ret = aw882xx_adapter_start();
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "AW882XX started\r\n");
    } else {
        shellPrint(shellGetCurrent(), "AW882XX start failed: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_start, shell_aw882xx_start, Start AW882XX SmartPA);

/**
 * @brief Stop AW882XX SmartPA
 */
static int shell_aw882xx_stop(int argc, char *argv[])
{
    int ret = aw882xx_adapter_stop();
    shellPrint(shellGetCurrent(), "AW882XX stopped\r\n");
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_stop, shell_aw882xx_stop, Stop AW882XX SmartPA);

/**
 * @brief Set AW882XX profile
 * @usage aw_profile <name>  (e.g., aw_profile Music)
 */
static int shell_aw882xx_profile(int argc, char *argv[])
{
    if (argc < 2) {
        shellPrint(shellGetCurrent(), "Usage: aw_profile <name>\r\n");
        shellPrint(shellGetCurrent(), "Available profiles: Music, Receiver\r\n");
        return -1;
    }
    
    int ret = aw882xx_set_profile(argv[1]);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Profile set to: %s\r\n", argv[1]);
    } else {
        shellPrint(shellGetCurrent(), "Failed to set profile: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 aw_profile, shell_aw882xx_profile, Set AW882XX profile);

/**
 * @brief Get/Set AW882XX volume
 * @usage aw_vol [value]  (get if no value, set if value provided)
 */
static int shell_aw882xx_volume(int argc, char *argv[])
{
    if (argc < 2) {
        /* Get volume */
        uint32_t volume = 0;
        int ret = aw882xx_adapter_get_volume(&volume);
        if (ret == 0) {
            shellPrint(shellGetCurrent(), "Current volume: %lu\r\n", volume);
        } else {
            shellPrint(shellGetCurrent(), "Failed to get volume: %d\r\n", ret);
        }
        return ret;
    } else {
        /* Set volume */
        uint32_t volume = (uint32_t)atoi(argv[1]);
        int ret = aw882xx_adapter_set_volume(volume);
        if (ret == 0) {
            shellPrint(shellGetCurrent(), "Volume set to: %lu\r\n", volume);
        } else {
            shellPrint(shellGetCurrent(), "Failed to set volume: %d\r\n", ret);
        }
        return ret;
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 aw_vol, shell_aw882xx_volume, Get or set AW882XX volume);

/**
 * @brief Dump AW882XX registers
 */
static int shell_aw882xx_reg(int argc, char *argv[])
{
    shellPrint(shellGetCurrent(), "Dumping AW882XX registers...\r\n");
    int ret = aw882xx_reg_show(AW_DEV_0);
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_reg, shell_aw882xx_reg, Dump AW882XX registers);

/**
 * @brief Write AW882XX register
 * @usage aw_wreg <addr> <value>  (hex format, e.g., aw_wreg 0x06 0x1234)
 */
static int shell_aw882xx_wreg(int argc, char *argv[])
{
    if (argc < 3) {
        shellPrint(shellGetCurrent(), "Usage: aw_wreg <addr> <value>\r\n");
        shellPrint(shellGetCurrent(), "Example: aw_wreg 0x06 0x1234\r\n");
        return -1;
    }
    
    uint8_t reg_addr = (uint8_t)strtol(argv[1], NULL, 0);
    uint16_t reg_data = (uint16_t)strtol(argv[2], NULL, 0);
    
    int ret = aw882xx_reg_store(AW_DEV_0, reg_addr, reg_data);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Write reg[0x%02X] = 0x%04X\r\n", reg_addr, reg_data);
    } else {
        shellPrint(shellGetCurrent(), "Failed to write register: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 aw_wreg, shell_aw882xx_wreg, Write AW882XX register);

/**
 * @brief Hardware reset AW882XX
 */
static int shell_aw882xx_reset(int argc, char *argv[])
{
    int ret = aw882xx_hw_reset_by_index(AW_DEV_0);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "AW882XX hardware reset done\r\n");
    } else {
        shellPrint(shellGetCurrent(), "AW882XX hardware reset failed: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_reset, shell_aw882xx_reset, Hardware reset AW882XX);

/**
 * @brief Soft reset AW882XX
 */
static int shell_aw882xx_soft_reset(int argc, char *argv[])
{
    int ret = aw882xx_soft_reset(AW_DEV_0);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "AW882XX soft reset done\r\n");
    } else {
        shellPrint(shellGetCurrent(), "AW882XX soft reset failed: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_sreset, shell_aw882xx_soft_reset, Soft reset AW882XX);

/**
 * @brief Show AW882XX driver version
 */
static int shell_aw882xx_version(int argc, char *argv[])
{
    char version[64] = {0};
    int ret = aw882xx_get_version(version, sizeof(version));
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "AW882XX driver version: %s\r\n", version);
    } else {
        shellPrint(shellGetCurrent(), "Failed to get version\r\n");
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_ver, shell_aw882xx_version, Show AW882XX driver version);

/**
 * @brief Show AW882XX help
 */
static int shell_aw882xx_help(int argc, char *argv[])
{
    shellPrint(shellGetCurrent(), "\r\n");
    shellPrint(shellGetCurrent(), "========== AW882XX SmartPA Commands ==========\r\n");
    shellPrint(shellGetCurrent(), "  aw_init    - Initialize SmartPA\r\n");
    shellPrint(shellGetCurrent(), "  aw_deinit  - Deinitialize SmartPA\r\n");
    shellPrint(shellGetCurrent(), "  aw_start   - Start SmartPA\r\n");
    shellPrint(shellGetCurrent(), "  aw_stop    - Stop SmartPA\r\n");
    shellPrint(shellGetCurrent(), "  aw_profile - Set audio profile (Music/Receiver)\r\n");
    shellPrint(shellGetCurrent(), "  aw_vol     - Get/Set volume\r\n");
    shellPrint(shellGetCurrent(), "  aw_reg     - Dump registers\r\n");
    shellPrint(shellGetCurrent(), "  aw_wreg    - Write register\r\n");
    shellPrint(shellGetCurrent(), "  aw_reset   - Hardware reset\r\n");
    shellPrint(shellGetCurrent(), "  aw_sreset  - Soft reset\r\n");
    shellPrint(shellGetCurrent(), "  aw_ver     - Show driver version\r\n");
    shellPrint(shellGetCurrent(), "==============================================\r\n");
    shellPrint(shellGetCurrent(), "\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_help, shell_aw882xx_help, Show AW882XX command help);
