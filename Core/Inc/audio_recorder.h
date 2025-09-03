/**
 * @file audio_recorder.h
 * @brief Audio recorder module for I2S TDM PCM data recording
 * @date 2025-01-28
 */

#ifndef __AUDIO_RECORDER_H__
#define __AUDIO_RECORDER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sai.h"
#include "fatfs.h"
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
    AUDIO_REC_IDLE = 0,
    AUDIO_REC_RECORDING,
    AUDIO_REC_STOPPING,
    AUDIO_REC_ERROR
} AudioRecorderState_t;

typedef struct {
    uint8_t channels;
    uint8_t bit_depth;
    uint32_t sample_rate;
    AudioRecorderState_t state;
    FIL file;
    char filename[64];
    uint32_t bytes_written;
    uint32_t buffer_size;
    bool file_open;
} AudioRecorder_t;

/* Exported constants --------------------------------------------------------*/
#define AUDIO_BUFFER_SIZE       (4096)  // Buffer size in bytes (multiple of frame size * 2), reduced for 4ch
#define AUDIO_CHANNELS          4       // Number of channels (changed from 8 to 4)
#define AUDIO_BIT_DEPTH         16      // Bits per sample
#define AUDIO_SAMPLE_RATE       16000   // Sample rate in Hz

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
int audio_recorder_init(void);
int audio_recorder_start(void);
int audio_recorder_stop(void);
AudioRecorderState_t audio_recorder_get_state(void);
uint32_t audio_recorder_get_bytes_written(void);
const char* audio_recorder_get_filename(void);
void audio_recorder_debug_status(void);
int audio_recorder_check_sd_card(void);

/* Callback functions */
void audio_recorder_rx_complete_callback(void);
void audio_recorder_rx_half_complete_callback(void);
void audio_recorder_error_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_RECORDER_H__ */