/**
 * @file audio_shell_cmd.c
 * @brief Shell commands for audio player control
 * @version 1.0.0
 * @date 2025-12-09
 */

#include "shell.h"
#include "audio_player.h"
#include "shell_log.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Play audio file
 * @usage play <filepath> [loop]
 * @example play /1khz.pcm
 * @example play /1khz.pcm loop
 */
static int shell_audio_play(int argc, char *argv[])
{
    if (argc < 2) {
        shellPrint(shellGetCurrent(), "Usage: play <filepath> [loop]\r\n");
        shellPrint(shellGetCurrent(), "Example: play /1khz.pcm\r\n");
        shellPrint(shellGetCurrent(), "Example: play /1khz.pcm loop\r\n");
        return -1;
    }
    
    const char *filepath = argv[1];
    bool loop = false;
    
    /* Check for loop option */
    if (argc >= 3 && strcmp(argv[2], "loop") == 0) {
        loop = true;
    }
    
    /* Default format: 32-bit, 48kHz, stereo */
    AudioFormat_t format = {
        .sample_rate = 48000,
        .bits_per_sample = 32,
        .channels = 2,
    };
    
    /* Initialize audio player if needed */
    static bool player_initialized = false;
    if (!player_initialized) {
        int ret = audio_player_init();
        if (ret != 0) {
            shellPrint(shellGetCurrent(), "Failed to initialize audio player: %d\r\n", ret);
            return ret;
        }
        player_initialized = true;
    }
    
    /* Start playback */
    int ret = audio_player_play_file(filepath, &format, loop);
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Playing: %s %s\r\n", filepath, loop ? "(looping)" : "");
    } else {
        shellPrint(shellGetCurrent(), "Failed to play file: %d\r\n", ret);
    }
    
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 play, shell_audio_play, Play PCM audio file);

/**
 * @brief Stop audio playback
 */
static int shell_audio_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    int ret = audio_player_stop();
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Playback stopped\r\n");
    } else {
        shellPrint(shellGetCurrent(), "Stop failed: %d\r\n", ret);
    }
    
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 stop, shell_audio_stop, Stop audio playback);

/**
 * @brief Pause audio playback
 */
static int shell_audio_pause(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    int ret = audio_player_pause();
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Playback paused\r\n");
    } else {
        shellPrint(shellGetCurrent(), "Pause failed: %d\r\n", ret);
    }
    
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 pause, shell_audio_pause, Pause audio playback);

/**
 * @brief Resume audio playback
 */
static int shell_audio_resume(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    int ret = audio_player_resume();
    if (ret == 0) {
        shellPrint(shellGetCurrent(), "Playback resumed\r\n");
    } else {
        shellPrint(shellGetCurrent(), "Resume failed: %d\r\n", ret);
    }
    
    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 resume, shell_audio_resume, Resume audio playback);

/**
 * @brief Show audio player status
 */
static int shell_audio_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    AudioPlayerStatus_t status;
    int ret = audio_player_get_status(&status);
    
    if (ret != 0) {
        shellPrint(shellGetCurrent(), "Failed to get status: %d\r\n", ret);
        return ret;
    }
    
    const char *state_str;
    switch (status.state) {
        case AUDIO_STATE_IDLE:    state_str = "IDLE"; break;
        case AUDIO_STATE_PLAYING: state_str = "PLAYING"; break;
        case AUDIO_STATE_PAUSED:  state_str = "PAUSED"; break;
        case AUDIO_STATE_STOPPED: state_str = "STOPPED"; break;
        case AUDIO_STATE_ERROR:   state_str = "ERROR"; break;
        default:                  state_str = "UNKNOWN"; break;
    }
    
    shellPrint(shellGetCurrent(), "\r\n");
    shellPrint(shellGetCurrent(), "===== Audio Player Status =====\r\n");
    shellPrint(shellGetCurrent(), "State:    %s\r\n", state_str);
    shellPrint(shellGetCurrent(), "Position: %lu / %lu ms\r\n", status.position_ms, status.duration_ms);
    shellPrint(shellGetCurrent(), "Samples:  %lu / %lu\r\n", status.played_samples, status.total_samples);
    shellPrint(shellGetCurrent(), "Loop:     %s\r\n", status.loop_enabled ? "ON" : "OFF");
    
    /* Show DMA stats */
    uint32_t half_count, full_count;
    audio_player_get_dma_stats(&half_count, &full_count);
    shellPrint(shellGetCurrent(), "DMA Half: %lu, Full: %lu\r\n", half_count, full_count);
    
    /* Show task stats */
    uint32_t fill_count, timeout_count;
    audio_player_get_task_stats(&fill_count, &timeout_count);
    shellPrint(shellGetCurrent(), "Task Fill: %lu, Timeout: %lu\r\n", fill_count, timeout_count);
    
    /* Show I/O stats */
    uint32_t read_count, memcpy_count;
    audio_player_get_io_stats(&read_count, &memcpy_count);
    shellPrint(shellGetCurrent(), "I/O Read: %lu, Memcpy: %lu\r\n", read_count, memcpy_count);
    
    /* Show queue stats */
    uint32_t q_send, q_fail;
    audio_player_get_queue_stats(&q_send, &q_fail);
    shellPrint(shellGetCurrent(), "Queue Send: %lu, Fail: %lu\r\n", q_send, q_fail);
    
    shellPrint(shellGetCurrent(), "===============================\r\n");
    shellPrint(shellGetCurrent(), "\r\n");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 astatus, shell_audio_status, Show audio player status);

/**
 * @brief Quick play 1kHz test tone
 */
static int shell_audio_play_1khz(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* Build args for play command */
    char *play_argv[] = {"play", "/1khz.pcm", "loop"};
    return shell_audio_play(3, play_argv);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 play1k, shell_audio_play_1khz, Play 1kHz test tone (loop));

/**
 * @brief Audio help
 */
static int shell_audio_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shellPrint(shellGetCurrent(), "\r\n");
    shellPrint(shellGetCurrent(), "========== Audio Player Commands ==========\r\n");
    shellPrint(shellGetCurrent(), "  play <file> [loop] - Play PCM audio file\r\n");
    shellPrint(shellGetCurrent(), "  stop               - Stop playback\r\n");
    shellPrint(shellGetCurrent(), "  pause              - Pause playback\r\n");
    shellPrint(shellGetCurrent(), "  resume             - Resume playback\r\n");
    shellPrint(shellGetCurrent(), "  astatus            - Show player status\r\n");
    shellPrint(shellGetCurrent(), "  play1k             - Play 1kHz test tone\r\n");
    shellPrint(shellGetCurrent(), "============================================\r\n");
    shellPrint(shellGetCurrent(), "\r\n");
    shellPrint(shellGetCurrent(), "Supported format: 32-bit, 48kHz, Stereo PCM\r\n");
    shellPrint(shellGetCurrent(), "\r\n");
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 ahelp, shell_audio_help, Show audio player help);
