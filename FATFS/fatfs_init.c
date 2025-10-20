/**
  ******************************************************************************
  * @file    fatfs_init.c
  * @brief   FatFs文件系统初始化函数
  * @date    2025-10-20
  ******************************************************************************
  */

#include "ff.h"
#include "sdmmc.h"
#include "debug.h"
#include <stdio.h>

/* 全局文件系统对象 */
FATFS SDFatFS;

/**
 * @brief  初始化FatFs文件系统
 * @retval 0=成功, -1=失败
 */
int fatfs_init(void)
{
    FRESULT res;
    
    DEBUG_INFO("========================================");
    DEBUG_INFO("  FatFs文件系统初始化");
    DEBUG_INFO("========================================");
    
    /* 1. 检查SD卡硬件状态 */
    DEBUG_INFO("检查SD卡硬件状态...");
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
    if (card_state != HAL_SD_CARD_TRANSFER) {
        DEBUG_ERROR("SD卡未就绪! 状态: %d", card_state);
        DEBUG_ERROR("请检查:");
        DEBUG_ERROR("  1. SD卡是否正确插入");
        DEBUG_ERROR("  2. SD卡是否损坏");
        DEBUG_ERROR("  3. SDMMC引脚连接");
        return -1;
    }
    DEBUG_INFO("✓ SD卡硬件就绪");
    
    /* 2. 挂载文件系统 */
    DEBUG_INFO("挂载文件系统...");
    res = f_mount(&SDFatFS, "SD:", 1);
    
    if (res != FR_OK) {
        DEBUG_ERROR("挂载失败! 错误码: %d", res);
        
        /* 如果是FR_NO_FILESYSTEM(13)，说明SD卡未格式化 */
        if (res == FR_NO_FILESYSTEM) {
            DEBUG_WARN("SD卡未格式化，正在格式化为FAT32...");
            
            BYTE work[FF_MAX_SS];  /* 工作缓冲区 */
            MKFS_PARM opt = {
                .fmt = FM_FAT32,   /* FAT32格式 */
                .n_fat = 0,        /* 自动选择FAT数量 */
                .align = 0,        /* 自动对齐 */
                .n_root = 0,       /* FAT32不使用此参数 */
                .au_size = 0       /* 自动选择分配单元大小 */
            };
            
            res = f_mkfs("SD:", &opt, work, sizeof(work));
            if (res == FR_OK) {
                DEBUG_INFO("✓ 格式化成功");
                
                /* 重新挂载 */
                res = f_mount(&SDFatFS, "SD:", 1);
                if (res == FR_OK) {
                    DEBUG_INFO("✓ 重新挂载成功");
                } else {
                    DEBUG_ERROR("重新挂载失败! 错误码: %d", res);
                    return -1;
                }
            } else {
                DEBUG_ERROR("格式化失败! 错误码: %d", res);
                return -1;
            }
        } else {
            /* 其他错误 */
            const char *error_msg;
            switch(res) {
                case FR_DISK_ERR:      error_msg = "磁盘底层错误"; break;
                case FR_INT_ERR:       error_msg = "内部错误"; break;
                case FR_NOT_READY:     error_msg = "磁盘未就绪"; break;
                case FR_INVALID_DRIVE: error_msg = "无效的驱动器号"; break;
                default:               error_msg = "未知错误"; break;
            }
            DEBUG_ERROR("挂载失败: %s", error_msg);
            return -1;
        }
    } else {
        DEBUG_INFO("✓ 文件系统挂载成功");
    }
    
    /* 3. 显示磁盘信息 */
    DEBUG_INFO("查询磁盘信息...");
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    res = f_getfree("SD:", &fre_clust, &fs);
    if (res == FR_OK) {
        /* 计算总空间和剩余空间（扇区） */
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        
        DEBUG_INFO("磁盘信息:");
        DEBUG_INFO("  文件系统: FAT%d", fs->fs_type);
        DEBUG_INFO("  总容量: %lu KB (%lu MB)", 
                   tot_sect / 2, tot_sect / 2048);
        DEBUG_INFO("  可用空间: %lu KB (%lu MB)", 
                   fre_sect / 2, fre_sect / 2048);
        DEBUG_INFO("  已使用: %.1f%%", 
                   100.0 * (tot_sect - fre_sect) / tot_sect);
        DEBUG_INFO("  扇区大小: %u bytes", FF_MAX_SS);
        DEBUG_INFO("  簇大小: %u 扇区", fs->csize);
    } else {
        DEBUG_WARN("无法获取磁盘信息 (错误码: %d)", res);
    }
    
    /* 4. 获取卷标（可选） */
    char label[12];
    DWORD vsn;
    res = f_getlabel("SD:", label, &vsn);
    if (res == FR_OK) {
        if (label[0]) {
            DEBUG_INFO("  卷标: %s", label);
        } else {
            DEBUG_INFO("  卷标: (无)");
        }
        DEBUG_INFO("  序列号: %08lX", vsn);
    }
    
    DEBUG_INFO("========================================");
    DEBUG_INFO("  FatFs初始化完成!");
    DEBUG_INFO("========================================");
    
    return 0;
}

/**
 * @brief  卸载FatFs文件系统
 * @note   在系统关闭或重启前调用
 */
void fatfs_deinit(void)
{
    DEBUG_INFO("卸载FatFs文件系统...");
    f_unmount("SD:");
    DEBUG_INFO("✓ FatFs已卸载");
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
    
    DEBUG_INFO("========================================");
    DEBUG_INFO("  FatFs简单测试");
    DEBUG_INFO("========================================");
    
    /* 创建测试文件 */
    DEBUG_INFO("创建测试文件: SD:/fatfs_test.txt");
    res = f_open(&fil, "SD:/fatfs_test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        DEBUG_ERROR("创建文件失败! 错误码: %d", res);
        return -1;
    }
    
    /* 写入测试数据 */
    const char *test_data = "=== FatFs Test File ===\n"
                           "Hello, FatFs!\n"
                           "STM32H750 + SD Card\n"
                           "Date: 2025-10-20\n";
    
    DEBUG_INFO("写入测试数据...");
    res = f_write(&fil, test_data, strlen(test_data), &bw);
    if (res != FR_OK) {
        DEBUG_ERROR("写入失败! 错误码: %d", res);
        f_close(&fil);
        return -1;
    }
    
    DEBUG_INFO("✓ 成功写入 %u 字节", bw);
    
    /* 关闭文件 */
    f_close(&fil);
    
    /* 读取文件验证 */
    DEBUG_INFO("读取文件验证...");
    res = f_open(&fil, "SD:/fatfs_test.txt", FA_READ);
    if (res != FR_OK) {
        DEBUG_ERROR("打开文件失败! 错误码: %d", res);
        return -1;
    }
    
    char read_buf[256];
    UINT br;
    res = f_read(&fil, read_buf, sizeof(read_buf) - 1, &br);
    if (res == FR_OK) {
        read_buf[br] = '\0';
        DEBUG_INFO("✓ 成功读取 %u 字节:", br);
        DEBUG_INFO("%s", read_buf);
    } else {
        DEBUG_ERROR("读取失败! 错误码: %d", res);
        f_close(&fil);
        return -1;
    }
    
    f_close(&fil);
    
    DEBUG_INFO("========================================");
    DEBUG_INFO("  FatFs测试通过!");
    DEBUG_INFO("========================================");
    
    return 0;
}
