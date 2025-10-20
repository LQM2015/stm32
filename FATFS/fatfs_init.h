/**
  ******************************************************************************
  * @file    fatfs_init.h
  * @brief   FatFs文件系统初始化函数头文件
  * @date    2025-10-20
  ******************************************************************************
  */

#ifndef __FATFS_INIT_H
#define __FATFS_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

/* 导出的全局文件系统对象 */
extern FATFS SDFatFS;

/* 函数声明 */
int fatfs_init(void);
void fatfs_deinit(void);
int fatfs_simple_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __FATFS_INIT_H */
