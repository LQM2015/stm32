/**
 * @file shell_cfg_user.h
 * @author Letter (nevermindzzt@gmail.com)
 * @brief shell config for STM32H725
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#ifndef __SHELL_CFG_USER_H__
#define __SHELL_CFG_USER_H__

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "portable.h"

/**
 * @brief 是否使用shell伴生对象
 *        一些扩展的组件(文件系统支持，日志工具等)需要使用伴生对象
 */
#define     SHELL_USING_COMPANION       1

/**
 * @brief 支持shell尾行模式
 */
#define     SHELL_SUPPORT_END_LINE      1

/**
 * @brief 使用LF作为命令行回车触发
 *        可以和SHELL_ENTER_CR同时开启
 */
#define     SHELL_ENTER_LF              1

/**
 * @brief 使用CR作为命令行回车触发
 *        可以和SHELL_ENTER_LF同时开启
 */
#define     SHELL_ENTER_CR              1

/**
 * @brief 使用CRLF作为命令行回车触发
 *        不可以和SHELL_ENTER_LF或SHELL_ENTER_CR同时开启
 */
#define     SHELL_ENTER_CRLF            0

/**
 * @brief shell格式化输入的缓冲大小
 *        为0时不使用shell格式化输入
 * @note shell格式化输入会阻塞shellTask, 仅适用于在有操作系统的情况下使用
 */
#define     SHELL_SCAN_BUFFER          128

/**
 * @brief 获取系统时间(ms)
 *        定义此宏为获取系统Tick，如`HAL_GetTick()`
 * @note 此宏不定义时无法使用双击tab补全命令help，无法使用shell超时锁定
 */
#define     SHELL_GET_TICK()            HAL_GetTick()

/**
 * @brief 使用锁
 * @note 使用shell锁时，需要对加锁和解锁进行实现
 */
#define     SHELL_USING_LOCK            1

/**
 * @brief shell内存分配
 *        shell本身不需要此接口，若使用shell伴生对象，需要进行定义
 */
#define     SHELL_MALLOC(size)          pvPortMalloc(size)

/**
 * @brief shell内存释放
 *        shell本身不需要此接口，若使用shell伴生对象，需要进行定义
 */
#define     SHELL_FREE(obj)             vPortFree(obj)

/**
 * @brief 是否显示shell信息
 */
#define     SHELL_SHOW_INFO             1

/**
 * @brief 是否在登录后清除命令行
 */
#define     SHELL_CLS_WHEN_LOGIN        1

/**
 * @brief shell默认用户
 */
#define     SHELL_DEFAULT_USER          "stm32h725"

/**
 * @brief shell默认用户密码
 *        若默认用户不需要密码，设为""
 */
#define     SHELL_DEFAULT_USER_PASSWORD ""

/**
 * @brief shell自动锁定超时
 *        shell当前用户密码有效的时候生效，超时后会自动重新锁定shell
 *        设置为0时关闭自动锁定功能，时间单位为`SHELL_GET_TICK()`单位
 * @note 使用超时锁定必须保证`SHELL_GET_TICK()`有效
 */
#define     SHELL_LOCK_TIMEOUT          0

/**
 * @brief shell命令最大长度
 */
#define     SHELL_COMMAND_MAX_LENGTH    128

/**
 * @brief shell命令参数最大数量
 */
#define     SHELL_PARAMETER_MAX_NUMBER  8

/**
 * @brief 历史命令记录数量
 */
#define     SHELL_HISTORY_MAX_NUMBER    8

/**
 * @brief shell格式化输出的缓冲大小
 */
#define     SHELL_PRINT_BUFFER          256

/**
 * @brief 使用函数签名
 */
#define     SHELL_USING_FUNC_SIGNATURE  1

/**
 * @brief 支持数组参数
 */
#define     SHELL_SUPPORT_ARRAY_PARAM   1

/**
 * @brief 执行未导出函数功能
 */
#define     SHELL_EXEC_UNDEF_FUNC       1

#endif /* __SHELL_CFG_USER_H__ */