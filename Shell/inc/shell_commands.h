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

/* =================================================================== */
/* Global Current Directory Management                                */
/* =================================================================== */
const char* get_current_directory(void);
void set_current_directory(const char* path);
void normalize_path(char* path);

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
int cmd_logctl(int argc, char *argv[]);
int cmd_logtest(int argc, char *argv[]);
int cmd_setclock(int argc, char *argv[]);

/* SD Card file operation commands */
int cmd_sdwrite(int argc, char *argv[]);
int cmd_sdread(int argc, char *argv[]);
int cmd_sdls(int argc, char *argv[]);
int cmd_sdrm(int argc, char *argv[]);

/* Linux-like filesystem commands */
int cmd_ls(int argc, char *argv[]);
int cmd_mkdir(int argc, char *argv[]);
int cmd_rm(int argc, char *argv[]);
int cmd_touch(int argc, char *argv[]);
int cmd_cp(int argc, char *argv[]);
int cmd_mv(int argc, char *argv[]);
int cmd_cat(int argc, char *argv[]);
int cmd_pwd(int argc, char *argv[]);
int cmd_cd(int argc, char *argv[]);
int cmd_df(int argc, char *argv[]);
int cmd_find(int argc, char *argv[]);

/* Additional text processing commands */
int cmd_wc(int argc, char *argv[]);
int cmd_head(int argc, char *argv[]);
int cmd_tail(int argc, char *argv[]);
int cmd_grep(int argc, char *argv[]);
int cmd_file(int argc, char *argv[]);
int cmd_du(int argc, char *argv[]);

/* Help commands */
int cmd_fshelp(int argc, char *argv[]);

/* Audio recording commands */
int cmd_audio_start(int argc, char *argv[]);
int cmd_audio_stop(int argc, char *argv[]);
int cmd_audio_reset(int argc, char *argv[]);
int cmd_audio_status(int argc, char *argv[]);

/* Additional utility commands */
int cmd_echo(int argc, char *argv[]);
int cmd_which(int argc, char *argv[]);
int cmd_clear(int argc, char *argv[]);
int cmd_history(int argc, char *argv[]);
int cmd_alias(int argc, char *argv[]);
int cmd_tree(int argc, char *argv[]);
int cmd_uptime(int argc, char *argv[]);
int cmd_date(int argc, char *argv[]);
int cmd_free(int argc, char *argv[]);

/* =================================================================== */
/* Common Aliases and Convenience Commands                            */
/* =================================================================== */
int cmd_ll(int argc, char *argv[]);
int cmd_la(int argc, char *argv[]);
int cmd_dir(int argc, char *argv[]);
int cmd_md(int argc, char *argv[]);
int cmd_rd(int argc, char *argv[]);
int cmd_del(int argc, char *argv[]);
int cmd_type(int argc, char *argv[]);
int cmd_more(int argc, char *argv[]);
int cmd_less(int argc, char *argv[]);
int cmd_fshelp_extended(int argc, char *argv[]);

/* Tab completion functions */
int shell_tab_completion(Shell *shell, char *buffer, int cursor_pos, int buffer_size);
void shell_set_tab_completion(Shell *shell);

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_COMMANDS_H__ */