/**
  ******************************************************************************
  * @file    fs_manager.h
  * @brief   Global File System Manager Header File
  ******************************************************************************
  * @attention
  *
  * 全局文件系统管理器，用于统一管理FATFS挂载/卸载操作
  * 避免在整个程序中重复调用f_mount
  * 
  ******************************************************************************
  */

#ifndef __FS_MANAGER_H
#define __FS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ff.h"
#include "fatfs.h"
#include "shell_log.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    FS_STATUS_NOT_MOUNTED = 0,
    FS_STATUS_MOUNTED,
    FS_STATUS_ERROR
} fs_status_t;

/* Exported constants --------------------------------------------------------*/
extern FATFS USERFatFS;
extern char USERPath[4];

/* Exported variables --------------------------------------------------------*/
extern fs_status_t g_fs_status;

/* Exported function prototypes ----------------------------------------------*/
/**
 * @brief 全局文件系统初始化（程序启动时调用一次）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_init(void);

/**
 * @brief 检查文件系统是否可用
 * @retval 0: 可用, -1: 不可用
 */
int fs_manager_check_status(void);

/**
 * @brief 获取当前文件系统状态
 * @retval fs_status_t 状态枚举
 */
fs_status_t fs_manager_get_status(void);

/**
 * @brief 强制重新挂载文件系统（仅在必要时使用）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_remount(void);

/**
 * @brief 卸载文件系统（程序结束时调用）
 * @retval 0: 成功, -1: 失败
 */
int fs_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __FS_MANAGER_H */
