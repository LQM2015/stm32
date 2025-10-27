/**
 * @file shell_spi_cmd.c
 * @brief Shell Command Interface for SPI Protocol
 * @version 1.0
 * @date 2025-01-22
 */

#include "shell.h"
#include "shell_log.h"
#include "spi_protocol_common.h"
#include "spi_protocol_media.h"
#include "spi_protocol_ota.h"
#include "spi_gpio_dispatcher.h"
#include <string.h>
#include <stdlib.h>

/* =================================================================== */
/* Helper Strings                                                     */
/* =================================================================== */

static const char spi_cmd_help[] = 
    "SPI Protocol Control Commands\r\n"
    "\r\n"
    "Usage:\r\n"
    "  spi help              - Show this help message\r\n"
    "  spi init              - Initialize SPI interface\r\n"
    "  spi photo             - Execute photo protocol\r\n"
    "  spi video             - Execute video protocol\r\n"
    "  spi media_auto        - Auto-detect and execute media protocol\r\n"
    "  spi ota_upgrade       - Execute OTA upgrade protocol\r\n"
    "  spi ota_transfer      - Execute OTA firmware transfer\r\n"
    "  spi dispatcher <cmd>  - Control GPIO dispatcher\r\n"
    "\r\n"
    "Dispatcher Commands:\r\n"
    "  init   - Initialize GPIO (PB12/PB6) and interrupts\r\n"
    "  start  - Start dispatcher thread\r\n"
    "  stop   - Stop dispatcher thread\r\n"
    "  enable - Enable auto event handling\r\n"
    "  disable- Disable auto event handling\r\n"
    "  status - Show current status\r\n"
    "\r\n"
    "Protocol Flows:\r\n"
    "  Photo:  [0xFE,0x01] -> [0xFD,0x0A] -> [0xFE,0x0A] -> [0x31,params] -> [0x32,0x01]\r\n"
    "  Video:  [0xFE,0x01] -> [0xFD,0x03] -> [0xFE,0x03] -> [0x13,params] -> [0x14,0x01]\r\n"
    "  OTA:    GPIO detection -> Upgrade -> Transfer firmware (state machine mode)\r\n"
    "\r\n";

/* =================================================================== */
/* Command Functions                                                  */
/* =================================================================== */

/**
 * @brief SPI command handler
 */
static int cmd_spi(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    
    if (argc < 2) {
        shellPrint(shell, spi_cmd_help);
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    /* Help command */
    if (strcmp(subcmd, "help") == 0) {
        shellPrint(shell, spi_cmd_help);
        return 0;
    }
    
    /* Init command */
    if (strcmp(subcmd, "init") == 0) {
        int ret = spi_protocol_init();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("SPI initialized successfully");
        } else {
            SHELL_LOG_USER_ERROR("SPI initialization failed: %d", ret);
        }
        return ret;
    }
    
    /* Photo protocol */
    if (strcmp(subcmd, "photo") == 0) {
        SHELL_LOG_USER_INFO("Executing photo protocol...");
        int ret = spi_protocol_photo_execute();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("Photo protocol completed successfully");
        } else {
            SHELL_LOG_USER_ERROR("Photo protocol failed: %d", ret);
        }
        return ret;
    }
    
    /* Video protocol */
    if (strcmp(subcmd, "video") == 0) {
        SHELL_LOG_USER_INFO("Executing video protocol...");
        int ret = spi_protocol_video_execute();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("Video protocol completed successfully");
        } else {
            SHELL_LOG_USER_ERROR("Video protocol failed: %d", ret);
        }
        return ret;
    }
    
    /* Media auto-detect */
    if (strcmp(subcmd, "media_auto") == 0) {
        SHELL_LOG_USER_INFO("Executing auto-detect media protocol...");
        int ret = spi_protocol_media_auto_execute();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("Media protocol completed successfully");
        } else {
            SHELL_LOG_USER_ERROR("Media protocol failed: %d", ret);
        }
        return ret;
    }
    
    /* OTA upgrade */
    if (strcmp(subcmd, "ota_upgrade") == 0) {
        SHELL_LOG_USER_INFO("Executing OTA upgrade protocol...");
        int ret = spi_protocol_ota_upgrade_execute();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("OTA upgrade protocol completed");
        } else {
            SHELL_LOG_USER_ERROR("OTA upgrade protocol failed: %d", ret);
        }
        return ret;
    }
    
    /* OTA firmware transfer */
    if (strcmp(subcmd, "ota_transfer") == 0) {
        SHELL_LOG_USER_INFO("Executing OTA firmware transfer (state machine mode)...");
        int ret = spi_protocol_ota_firmware_transfer_execute();
        if (ret == 0) {
            SHELL_LOG_USER_INFO("OTA firmware transfer completed");
        } else {
            SHELL_LOG_USER_ERROR("OTA firmware transfer failed: %d", ret);
        }
        return ret;
    }
    
    /* GPIO Dispatcher control */
    if (strcmp(subcmd, "dispatcher") == 0) {
        if (argc < 3) {
            SHELL_LOG_USER_ERROR("Usage: spi dispatcher <init|start|stop|enable|disable|status>");
            return -1;
        }
        
        const char *disp_cmd = argv[2];
        
        if (strcmp(disp_cmd, "init") == 0) {
            int ret = spi_gpio_dispatcher_init();
            if (ret == 0) {
                SHELL_LOG_USER_INFO("GPIO Dispatcher initialized (GPIO PB12/PB6 configured)");
            } else {
                SHELL_LOG_USER_ERROR("Failed to initialize dispatcher: %d", ret);
            }
            return ret;
        }
        else if (strcmp(disp_cmd, "start") == 0) {
            int ret = spi_gpio_dispatcher_start();
            if (ret == 0) {
                SHELL_LOG_USER_INFO("GPIO Dispatcher thread started");
            } else {
                SHELL_LOG_USER_ERROR("Failed to start dispatcher: %d", ret);
            }
            return ret;
        }
        else if (strcmp(disp_cmd, "stop") == 0) {
            int ret = spi_gpio_dispatcher_stop();
            if (ret == 0) {
                SHELL_LOG_USER_INFO("GPIO Dispatcher thread stopped");
            } else {
                SHELL_LOG_USER_ERROR("Failed to stop dispatcher: %d", ret);
            }
            return ret;
        }
        else if (strcmp(disp_cmd, "enable") == 0) {
            spi_gpio_dispatcher_enable(true);
            SHELL_LOG_USER_INFO("GPIO Dispatcher auto-handling enabled");
            return 0;
        }
        else if (strcmp(disp_cmd, "disable") == 0) {
            spi_gpio_dispatcher_enable(false);
            SHELL_LOG_USER_INFO("GPIO Dispatcher auto-handling disabled");
            return 0;
        }
        else if (strcmp(disp_cmd, "status") == 0) {
            bool enabled = spi_gpio_dispatcher_is_enabled();
            SHELL_LOG_USER_INFO("GPIO Dispatcher: %s", enabled ? "Enabled" : "Disabled");
            return 0;
        }
        else {
            SHELL_LOG_USER_ERROR("Unknown dispatcher command: %s", disp_cmd);
            SHELL_LOG_USER_ERROR("Available: init, start, stop, enable, disable, status");
            return -1;
        }
    }
    
    /* Unknown command */
    SHELL_LOG_USER_ERROR("Unknown subcommand: %s", subcmd);
    shellPrint(shell, "Use 'spi help' for usage information\r\n");
    return -1;
}

/* Export shell command */
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 spi, cmd_spi, "SPI protocol control commands");
