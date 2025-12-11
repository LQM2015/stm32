/**
 * @file audio_player.h
 * @brief Audio player for PCM files via I2S/SmartPA
 * @version 1.0.0
 * @date 2025-12-09
 */

#ifndef __AUDIO_PLAYER_H__
#define __AUDIO_PLAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/********************************************
 * Audio Player Configuration
 *******************************************/

/* Double buffer size for DMA (samples per buffer, not bytes) */
/* Each sample = 2 channels * 4 bytes (32-bit) = 8 bytes */
/* 4096 samples = 32768 bytes per half buffer (~85ms @ 48kHz) */
#define AUDIO_BUFFER_SAMPLES    4096
#define AUDIO_BUFFER_SIZE       (AUDIO_BUFFER_SAMPLES * 2)  /* 32-bit stereo = 2 x uint32_t per sample */

/* Duration of half buffer in ms (approx) for monitoring */
/* 4096 samples / 48000 Hz * 1000 = 85.33 ms */
#define AUDIO_BUFFER_HALF_DURATION_MS 85

/********************************************
 * Audio Player States
 *******************************************/
typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_ERROR,
} AudioState_t;

/********************************************
 * Audio Format Configuration
 *******************************************/
typedef struct {
    uint32_t sample_rate;       /* Sample rate in Hz (e.g., 48000) */
    uint8_t  bits_per_sample;   /* Bits per sample (16, 24, 32) */
    uint8_t  channels;          /* Number of channels (1=mono, 2=stereo) */
    bool     is_float;          /* True if data is 32-bit float */
} AudioFormat_t;

/********************************************
 * Audio Player Status
 *******************************************/
typedef struct {
    AudioState_t state;
    uint32_t total_samples;     /* Total samples in file */
    uint32_t played_samples;    /* Samples already played */
    uint32_t duration_ms;       /* Total duration in ms */
    uint32_t position_ms;       /* Current position in ms */
    bool     loop_enabled;      /* Loop playback enabled */
} AudioPlayerStatus_t;

/********************************************
 * Public API Functions
 *******************************************/

/**
 * @brief Initialize audio player
 * @return 0 on success, negative error code on failure
 */
int audio_player_init(void);

/**
 * @brief Deinitialize audio player
 */
void audio_player_deinit(void);

/**
 * @brief Play PCM file from filesystem
 * @param filepath Path to PCM file (e.g., "/1khz.pcm")
 * @param format Audio format configuration
 * @param loop Enable loop playback
 * @return 0 on success, negative error code on failure
 */
int audio_player_play_file(const char *filepath, AudioFormat_t *format, bool loop);

/**
 * @brief Stop playback
 * @return 0 on success, negative error code on failure
 */
int audio_player_stop(void);

/**
 * @brief Pause playback
 * @return 0 on success, negative error code on failure
 */
int audio_player_pause(void);

/**
 * @brief Resume playback
 * @return 0 on success, negative error code on failure
 */
int audio_player_resume(void);

/**
 * @brief Get player status
 * @param status Pointer to status structure
 * @return 0 on success, negative error code on failure
 */
int audio_player_get_status(AudioPlayerStatus_t *status);

/**
 * @brief Check if player is playing
 * @return true if playing, false otherwise
 */
bool audio_player_is_playing(void);

/**
 * @brief Get DMA callback counters for debugging
 * @param half_count Pointer to half transfer count (can be NULL)
 * @param full_count Pointer to full transfer count (can be NULL)
 */
void audio_player_get_dma_stats(uint32_t *half_count, uint32_t *full_count);

/**
 * @brief Get task statistics for debugging
 * @param fill_count Pointer to buffer fill count (can be NULL)
 * @param timeout_count Pointer to semaphore timeout count (can be NULL)
 */
void audio_player_get_task_stats(uint32_t *fill_count, uint32_t *timeout_count);

/**
 * @brief Get I/O statistics for debugging
 * @param read_count Pointer to f_read call count (can be NULL)
 * @param memcpy_count Pointer to memcpy call count (can be NULL)
 */
void audio_player_get_io_stats(uint32_t *read_count, uint32_t *memcpy_count);

/**
 * @brief Get queue statistics for debugging
 * @param send_count Pointer to queue send count (can be NULL)
 * @param fail_count Pointer to queue send failure count (can be NULL)
 */
void audio_player_get_queue_stats(uint32_t *send_count, uint32_t *fail_count);

/**
 * @brief I2S DMA half transfer complete callback (called from ISR)
 */
void audio_player_half_transfer_callback(void);

/**
 * @brief I2S DMA transfer complete callback (called from ISR)
 */
void audio_player_transfer_complete_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_PLAYER_H__ */
