/**
  ******************************************************************************
  * @file    fatfs_init.c
  * @brief   FatFs文件系统初始化函数
  * @date    2025-10-20
  ******************************************************************************
  */

#include "ff.h"
#include "sdmmc.h"
#include "diskio.h"
#include "debug.h"
#include "shell_log.h"  /* SHELL_LOG_FATFS_xxx macros */
#include <stdio.h>
#include <stdint.h>

/* 全局文件系统对象 */
FATFS SDFatFS;

/* FatFs工作对象 - 用于Shell命令的文件操作 */
FIL USERFile;      /* File object */
char USERPath[4];  /* Logical drive path */

/**
 * @brief  初始化FatFs文件系统
 * @retval 0=成功, -1=失败
 */
int fatfs_init(void)
{
    FRESULT res;
    
    /* 初始化USERPath */
    USERPath[0] = 'S';
    USERPath[1] = 'D';
    USERPath[2] = ':';
    USERPath[3] = '\0';
    
    SHELL_LOG_FATFS_INFO("========================================");
    SHELL_LOG_FATFS_INFO("  FatFs Initialization");
    SHELL_LOG_FATFS_INFO("========================================");
    
    /* 1. 检查SD卡硬件状态 */
    SHELL_LOG_FATFS_INFO("检查SD卡硬件状态...");
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
    if (card_state != HAL_SD_CARD_TRANSFER) {
        SHELL_LOG_FATFS_ERROR("SD卡未就绪! 状态: %d", card_state);
        SHELL_LOG_FATFS_ERROR("请检查:");
        SHELL_LOG_FATFS_ERROR("  1. SD卡是否正确插入");
        SHELL_LOG_FATFS_ERROR("  2. SD卡是否损坏");
        SHELL_LOG_FATFS_ERROR("  3. SDMMC引脚连接");
        return -1;
    }
    SHELL_LOG_FATFS_INFO("✓ SD卡硬件就绪");
    
    /* 2. 挂载文件系统 */
    SHELL_LOG_FATFS_INFO("挂载文件系统...");
    /* Try mount once first */
    res = f_mount(&SDFatFS, "SD:", 1);
    
    if (res != FR_OK) {
        SHELL_LOG_FATFS_ERROR("Mount failed: %d", res);
        
        /* Try formatting if no filesystem found */
        if (res == FR_NO_FILESYSTEM) {
            SHELL_LOG_FATFS_WARNING("Formatting SD card as FAT32...");
            
            BYTE work[FF_MAX_SS];
            MKFS_PARM opt = {
                .fmt = FM_FAT32,
                .n_fat = 0,
                .align = 0,
                .n_root = 0,
                .au_size = 0
            };
            
            res = f_mkfs("SD:", &opt, work, sizeof(work));
            if (res == FR_OK) {
                SHELL_LOG_FATFS_INFO("Format OK");
                res = f_mount(&SDFatFS, "SD:", 1);
                if (res != FR_OK) {
                    SHELL_LOG_FATFS_ERROR("Remount failed: %d", res);
                    return -1;
                }
            } else {
                SHELL_LOG_FATFS_ERROR("Format failed: %d", res);
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    SHELL_LOG_FATFS_INFO("Mount OK");
    
    /* Get disk info */
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    res = f_getfree("SD:", &fre_clust, &fs);
    if (res == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        
        SHELL_LOG_FATFS_INFO("FAT%d: %lu MB total, %lu MB free", 
                   fs->fs_type, tot_sect / 2048, fre_sect / 2048);
    }
    
    SHELL_LOG_FATFS_INFO("FatFs ready");
    
    return 0;
}

/**
 * @brief  卸载FatFs文件系统
 * @note   在系统关闭或重启前调用
 */
void fatfs_deinit(void)
{
    SHELL_LOG_FATFS_INFO("卸载FatFs文件系统...");
    f_unmount("SD:");
    SHELL_LOG_FATFS_INFO("✓ FatFs已卸载");
}

/**
 * @brief  简单的FatFs测试函数
 * @note   创建一个测试文件并写入内容
 * @retval 0=成功, -1=失败
 */
int fatfs_simple_test(void)
{
    FRESULT res;
    FIL fil;
    UINT bw;
    
    SHELL_LOG_FATFS_INFO("========================================");
    SHELL_LOG_FATFS_INFO("  FatFs简单测试");
    SHELL_LOG_FATFS_INFO("========================================");
    
    /* 创建测试文件 */
    SHELL_LOG_FATFS_INFO("创建测试文件: SD:/fatfs_test.txt");
    res = f_open(&fil, "SD:/fatfs_test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        SHELL_LOG_FATFS_ERROR("创建文件失败! 错误码: %d", res);
        return -1;
    }
    
    /* 写入测试数据 */
    const char *test_data = "=== FatFs Test File ===\n"
                           "Hello, FatFs!\n"
                           "STM32H750 + SD Card\n"
                           "Date: 2025-10-20\n";
    
    SHELL_LOG_FATFS_INFO("写入测试数据...");
    res = f_write(&fil, test_data, strlen(test_data), &bw);
    if (res != FR_OK) {
        SHELL_LOG_FATFS_ERROR("写入失败! 错误码: %d", res);
        f_close(&fil);
        return -1;
    }
    
    SHELL_LOG_FATFS_INFO("✓ 成功写入 %u 字节", bw);
    
    /* 关闭文件 */
    f_close(&fil);
    
    /* 读取文件验证 */
    SHELL_LOG_FATFS_INFO("读取文件验证...");
    res = f_open(&fil, "SD:/fatfs_test.txt", FA_READ);
    if (res != FR_OK) {
        SHELL_LOG_FATFS_ERROR("打开文件失败! 错误码: %d", res);
        return -1;
    }
    
    char read_buf[256];
    UINT br;
    res = f_read(&fil, read_buf, sizeof(read_buf) - 1, &br);
    if (res == FR_OK) {
        read_buf[br] = '\0';
        SHELL_LOG_FATFS_INFO("✓ 成功读取 %u 字节:", br);
        SHELL_LOG_FATFS_INFO("%s", read_buf);
    } else {
        SHELL_LOG_FATFS_ERROR("读取失败! 错误码: %d", res);
        f_close(&fil);
        return -1;
    }
    
    f_close(&fil);
    
    SHELL_LOG_FATFS_INFO("========================================");
    SHELL_LOG_FATFS_INFO("  FatFs测试通过!");
    SHELL_LOG_FATFS_INFO("========================================");
    
    return 0;
}
