/**
  ******************************************************************************
  * @file    fs_manager.c
  * @brief   Global File System Manager Implementation
  ******************************************************************************
  * @attention
  *
  * 全局文件系统管理器实现，用于统一管理FATFS挂载/卸载操作
  * 避免在整个程序中重复调用f_mount
  * 
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "fs_manager.h"

/* Private variables ---------------------------------------------------------*/
fs_status_t g_fs_status = FS_STATUS_NOT_MOUNTED;

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 全局文件系统初始化（程序启动时调用一次）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_init(void)
{
    FRESULT res;
    FATFS *fs;
    DWORD fre_clust;
    
    SHELL_LOG_SYS_INFO("Initializing global file system manager...");
    
    // 确保之前没有挂载
    g_fs_status = FS_STATUS_NOT_MOUNTED;
    
    // 执行一次性挂载
    res = f_mount(&USERFatFS, USERPath, 1);
    if (res != FR_OK) {
        SHELL_LOG_SYS_ERROR("Failed to mount file system, FRESULT: %d", res);
        
        // 详细的错误信息
        switch(res) {
            case FR_NOT_READY:
                SHELL_LOG_SYS_ERROR("Drive not ready - SD card may not be inserted");
                break;
            case FR_DISK_ERR:
                SHELL_LOG_SYS_ERROR("Disk error - SD card hardware issue");
                break;
            case FR_NOT_ENABLED:
                SHELL_LOG_SYS_ERROR("Volume not enabled");
                break;
            case FR_NO_FILESYSTEM:
                SHELL_LOG_SYS_ERROR("No valid filesystem found on SD card");
                break;
            case FR_INVALID_DRIVE:
                SHELL_LOG_SYS_ERROR("Invalid drive path");
                break;
            default:
                SHELL_LOG_SYS_ERROR("Unknown mount error: %d", res);
                break;
        }
        
        g_fs_status = FS_STATUS_ERROR;
        return -1;
    }
    
    // 验证挂载是否成功
    res = f_getfree(USERPath, &fre_clust, &fs);
    if (res != FR_OK) {
        SHELL_LOG_SYS_ERROR("File system verification failed, FRESULT: %d", res);
        g_fs_status = FS_STATUS_ERROR;
        return -1;
    }
    
    g_fs_status = FS_STATUS_MOUNTED;
    SHELL_LOG_SYS_INFO("File system mounted successfully");
    SHELL_LOG_SYS_INFO("Free clusters: %lu, cluster size: %lu bytes", 
                       fre_clust, fs->csize * 512);
    SHELL_LOG_SYS_INFO("Free space: %lu MB", (fre_clust * fs->csize) / 2048);
    
    return 0;
}

/**
 * @brief 检查文件系统是否可用
 * @retval 0: 可用, -1: 不可用
 */
int fs_manager_check_status(void)
{
    if (g_fs_status != FS_STATUS_MOUNTED) {
        return -1;
    }
    
    // 快速验证：尝试获取空闲空间
    FATFS *fs;
    DWORD fre_clust;
    FRESULT res = f_getfree(USERPath, &fre_clust, &fs);
    
    if (res != FR_OK) {
        SHELL_LOG_SYS_WARNING("File system check failed, FRESULT: %d", res);
        g_fs_status = FS_STATUS_ERROR;
        return -1;
    }
    
    return 0;
}

/**
 * @brief 获取当前文件系统状态
 * @retval fs_status_t 状态枚举
 */
fs_status_t fs_manager_get_status(void)
{
    return g_fs_status;
}

/**
 * @brief 强制重新挂载文件系统（仅在必要时使用）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_remount(void)
{
    FRESULT res;
    FATFS *fs;
    DWORD fre_clust;
    
    SHELL_LOG_SYS_WARNING("Force remounting file system...");
    
    // 先卸载
    f_mount(NULL, USERPath, 0);
    g_fs_status = FS_STATUS_NOT_MOUNTED;
    
    // 短暂延时
    HAL_Delay(200);
    
    // 重新挂载
    res = f_mount(&USERFatFS, USERPath, 1);
    if (res != FR_OK) {
        SHELL_LOG_SYS_ERROR("Failed to remount file system, FRESULT: %d", res);
        g_fs_status = FS_STATUS_ERROR;
        return -1;
    }
    
    // 验证
    res = f_getfree(USERPath, &fre_clust, &fs);
    if (res != FR_OK) {
        SHELL_LOG_SYS_ERROR("File system remount verification failed, FRESULT: %d", res);
        g_fs_status = FS_STATUS_ERROR;
        return -1;
    }
    
    g_fs_status = FS_STATUS_MOUNTED;
    SHELL_LOG_SYS_INFO("File system remounted successfully");
    
    return 0;
}

/**
 * @brief 卸载文件系统（程序结束时调用）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_deinit(void)
{
    SHELL_LOG_SYS_INFO("Deinitializing file system manager...");
    
    // 卸载文件系统
    f_mount(NULL, USERPath, 0);
    g_fs_status = FS_STATUS_NOT_MOUNTED;
    
    SHELL_LOG_SYS_INFO("File system unmounted");
    return 0;
}
