/**
 * @file shell_port.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell port header for STM32H725 with FreeRTOS
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#ifndef __SHELL_PORT_H__
#define __SHELL_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include "shell.h"
#include <stdarg.h>

/* Exported constants --------------------------------------------------------*/
#define SHELL_UART                  &huart3
#define SHELL_BUFFER_SIZE           512
#define SHELL_RX_QUEUE_SIZE         64
#define SHELL_TASK_STACK_SIZE       2048
#define SHELL_TASK_PRIORITY         3

/* Exported types ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

/* Exported functions prototypes ---------------------------------------------*/
void shell_init(void);
Shell* shell_get_instance(void);
void shell_printf(const char *fmt, ...);
int shell_exec(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_PORT_H__ */