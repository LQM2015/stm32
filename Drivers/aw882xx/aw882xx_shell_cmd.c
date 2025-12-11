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
#include "FreeRTOS.h"
#include "task.h"

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
 * @brief Read AW882XX register
 * @usage aw_rreg <addr>  (hex format, e.g., aw_rreg 0x06)
 */
static int shell_aw882xx_rreg(int argc, char *argv[])
{
    if (argc < 2) {
        shellPrint(shellGetCurrent(), "Usage: aw_rreg <addr>\r\n");
        shellPrint(shellGetCurrent(), "Example: aw_rreg 0x06\r\n");
        return -1;
    }
    
    uint8_t reg_addr = (uint8_t)strtol(argv[1], NULL, 0);
    unsigned int reg_data = 0;
    
    /* Get the AW882XX device and read register */
    extern struct aw882xx *g_aw882xx[AW_DEV_MAX];
    struct aw882xx *aw882xx = g_aw882xx[AW_DEV_0];
    
    if (aw882xx == NULL) {
        shellPrint(shellGetCurrent(), "AW882XX not initialized\r\n");
        return -1;
    }
    
    int ret = aw882xx_i2c_read(aw882xx, reg_addr, &reg_data);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Read reg[0x%02X] = 0x%04X\r\n", reg_addr, reg_data);
    } else {
        shellPrint(shellGetCurrent(), "Failed to read register: %d\r\n", ret);
    }
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 aw_rreg, shell_aw882xx_rreg, Read AW882XX register);

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
 * @brief Scan I2C bus for devices
 */
#include "i2c.h"
extern I2C_HandleTypeDef hi2c1;

static int shell_aw882xx_i2c_scan(int argc, char *argv[])
{
    shellPrint(shellGetCurrent(), "\r\nScanning I2C1 bus...\r\n");
    shellPrint(shellGetCurrent(), "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    
    int found = 0;
    for (uint8_t row = 0; row < 8; row++) {
        shellPrint(shellGetCurrent(), "%02X: ", row << 4);
        for (uint8_t col = 0; col < 16; col++) {
            uint8_t addr = (row << 4) | col;
            
            /* Skip reserved addresses */
            if (addr < 0x08 || addr > 0x77) {
                shellPrint(shellGetCurrent(), "   ");
                continue;
            }
            
            /* Try to communicate with device */
            HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, (addr << 1), 1, 10);
            if (status == HAL_OK) {
                shellPrint(shellGetCurrent(), "%02X ", addr);
                found++;
            } else {
                shellPrint(shellGetCurrent(), "-- ");
            }
        }
        shellPrint(shellGetCurrent(), "\r\n");
    }
    
    shellPrint(shellGetCurrent(), "\r\nFound %d device(s)\r\n", found);
    shellPrint(shellGetCurrent(), "Expected AW882XX at: 0x34 or 0x35\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_scan, shell_aw882xx_i2c_scan, Scan I2C bus for devices);

/**
 * @brief Monitor AW882XX status registers for debugging
 * Usage: aw_mon [count] [interval_ms]
 */
static int shell_aw882xx_monitor(int argc, char *argv[])
{
    extern struct aw882xx *g_aw882xx[AW_DEV_MAX];
    struct aw882xx *aw882xx = g_aw882xx[AW_DEV_0];
    
    if (aw882xx == NULL) {
        shellPrint(shellGetCurrent(), "Error: Device not initialized\r\n");
        return -1;
    }
    
    int count = 10;  // default 10 iterations
    int interval_ms = 100;  // default 100ms interval
    
    if (argc >= 2) {
        count = atoi(argv[1]);
        if (count <= 0) count = 10;
    }
    if (argc >= 3) {
        interval_ms = atoi(argv[2]);
        if (interval_ms <= 0) interval_ms = 100;
    }
    
    shellPrint(shellGetCurrent(), "Monitoring %d times, interval %dms\r\n", count, interval_ms);
    shellPrint(shellGetCurrent(), "SYSST: PLLS(0)=Lock CLKS(4)=Stable NOCLKS(5)=NoClock\r\n");
    shellPrint(shellGetCurrent(), "----------------------------------------\r\n");
    
    unsigned int sysst, sysint, i2sctrl1, sysctrl, i2scapcnt;
    
    for (int i = 0; i < count; i++) {
        aw882xx_i2c_read(aw882xx, 0x01, &sysst);       // SYSST
        aw882xx_i2c_read(aw882xx, 0x02, &sysint);      // SYSINT (interrupts)
        aw882xx_i2c_read(aw882xx, 0x06, &i2sctrl1);    // I2SCTRL1
        aw882xx_i2c_read(aw882xx, 0x04, &sysctrl);     // SYSCTRL
        aw882xx_i2c_read(aw882xx, 0x27, &i2scapcnt);   // I2SCAPCNT
        
        int plls = (sysst >> 0) & 0x01;    // bit 0
        int clks = (sysst >> 4) & 0x01;    // bit 4
        int noclks = (sysst >> 5) & 0x01;  // bit 5
        int pwdn = (sysctrl >> 0) & 0x01;  // bit 0
        int amppd = (sysctrl >> 1) & 0x01; // bit 1
        
        shellPrint(shellGetCurrent(), "[%02d] ST=0x%04X(PLL=%d CLK=%d NC=%d) INT=0x%04X I2S=0x%04X CAP=0x%04X CTL=0x%04X(PD=%d AMP=%d)\r\n",
                   i, sysst, plls, clks, noclks, sysint, i2sctrl1, i2scapcnt, sysctrl, pwdn, amppd);
        
        if (interval_ms > 0 && i < count - 1) {
            vTaskDelay(pdMS_TO_TICKS(interval_ms));
        }
    }
    
    shellPrint(shellGetCurrent(), "----------------------------------------\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_mon, shell_aw882xx_monitor, Monitor status registers: aw_mon [count] [interval_ms]);

/**
 * @brief Try to recover PLL lock by re-writing I2SCTRL1 and toggling power
 */
static int shell_aw882xx_recover(int argc, char *argv[])
{
    extern struct aw882xx *g_aw882xx[AW_DEV_MAX];
    struct aw882xx *aw882xx = g_aw882xx[AW_DEV_0];
    
    if (aw882xx == NULL) {
        shellPrint(shellGetCurrent(), "Error: Device not initialized\r\n");
        return -1;
    }
    
    unsigned int sysst, i2sctrl1;
    
    // Read current state
    aw882xx_i2c_read(aw882xx, 0x01, &sysst);
    aw882xx_i2c_read(aw882xx, 0x06, &i2sctrl1);
    shellPrint(shellGetCurrent(), "Before: SYSST=0x%04X, I2SCTRL1=0x%04X\r\n", sysst, i2sctrl1);
    
    // Method 1: Just re-write I2SCTRL1
    shellPrint(shellGetCurrent(), "Writing I2SCTRL1=0x34E8...\r\n");
    aw882xx_i2c_write(aw882xx, 0x06, 0x34E8);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    aw882xx_i2c_read(aw882xx, 0x01, &sysst);
    aw882xx_i2c_read(aw882xx, 0x06, &i2sctrl1);
    shellPrint(shellGetCurrent(), "After I2S write: SYSST=0x%04X, I2SCTRL1=0x%04X\r\n", sysst, i2sctrl1);
    
    if ((sysst & 0x11) == 0x11) {
        shellPrint(shellGetCurrent(), "PLL locked! (PLLS=1, CLKS=1)\r\n");
        return 0;
    }
    
    // Method 2: Toggle PWDN
    shellPrint(shellGetCurrent(), "Toggling power...\r\n");
    unsigned int sysctrl;
    aw882xx_i2c_read(aw882xx, 0x04, &sysctrl);
    aw882xx_i2c_write(aw882xx, 0x04, sysctrl | 0x01);  // PWDN=1
    vTaskDelay(pdMS_TO_TICKS(5));
    aw882xx_i2c_write(aw882xx, 0x04, sysctrl & ~0x01); // PWDN=0
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Re-write I2SCTRL1 after power cycle
    aw882xx_i2c_write(aw882xx, 0x06, 0x34E8);
    vTaskDelay(pdMS_TO_TICKS(20));
    
    aw882xx_i2c_read(aw882xx, 0x01, &sysst);
    aw882xx_i2c_read(aw882xx, 0x06, &i2sctrl1);
    shellPrint(shellGetCurrent(), "After power toggle: SYSST=0x%04X, I2SCTRL1=0x%04X\r\n", sysst, i2sctrl1);
    
    if ((sysst & 0x11) == 0x11) {
        shellPrint(shellGetCurrent(), "PLL locked!\r\n");
        return 0;
    }
    
    shellPrint(shellGetCurrent(), "Recovery failed - PLL still not locked\r\n");
    return -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_recover, shell_aw882xx_recover, Try to recover PLL lock);

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
    shellPrint(shellGetCurrent(), "  aw_rreg    - Read register\r\n");
    shellPrint(shellGetCurrent(), "  aw_wreg    - Write register\r\n");
    shellPrint(shellGetCurrent(), "  aw_reset   - Hardware reset\r\n");
    shellPrint(shellGetCurrent(), "  aw_sreset  - Soft reset\r\n");
    shellPrint(shellGetCurrent(), "  aw_ver     - Show driver version\r\n");
    shellPrint(shellGetCurrent(), "  aw_scan    - Scan I2C bus for devices\r\n");
    shellPrint(shellGetCurrent(), "  aw_mon     - Monitor status registers\r\n");
    shellPrint(shellGetCurrent(), "  aw_recover - Try to recover PLL lock\r\n");
    shellPrint(shellGetCurrent(), "==============================================\r\n");
    shellPrint(shellGetCurrent(), "\r\n");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aw_help, shell_aw882xx_help, Show AW882XX command help);
