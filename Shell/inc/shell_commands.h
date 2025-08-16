/**
 * @file shell_commands.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief Shell commands header for STM32H725 project
 * @version 3.2.4
 * @date 2025-01-16
 * 
 * @copyright (c) 2025 Letter
 * 
 */

#ifndef __SHELL_COMMANDS_H__
#define __SHELL_COMMANDS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "shell.h"

/* Exported functions prototypes ---------------------------------------------*/
int cmd_sysinfo(int argc, char *argv[]);
int cmd_meminfo(int argc, char *argv[]);
int cmd_taskinfo(int argc, char *argv[]);
int cmd_reboot(int argc, char *argv[]);
int cmd_clear(int argc, char *argv[]);
int cmd_led(int argc, char *argv[]);
int cmd_clocktest(int argc, char *argv[]);
int cmd_version(int argc, char *argv[]);
int cmd_hexdump(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_COMMANDS_H__ */