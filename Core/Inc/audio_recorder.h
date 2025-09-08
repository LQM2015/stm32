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
    // 文件对象分离，避免被音频录制器其他部分踩踏
    // FIL file;                   // 移除嵌入的文件对象
    char filename[64];
    uint32_t bytes_written;
    uint32_t buffer_size;
    bool file_open;
    bool write_in_progress;  // Flag to prevent reentrant writes
} __attribute__((aligned(32))) AudioRecorder_t;  // 32字节对齐，匹配缓存行

// 独立的文件对象，不与录音器结构体混在一起
// extern FIL g_audio_file __attribute__((section(".dma_buffer")));  // 放在DMA安全区域
// 改为在源文件中定义为static，不需要extern声明

/* Exported constants --------------------------------------------------------*/
#define AUDIO_CHANNELS          8       // Number of channels (configurable: 4 or 8)
#define AUDIO_BIT_DEPTH         16      // Bits per sample (fixed at 16-bit)
#define AUDIO_SAMPLE_RATE       16000   // Sample rate in Hz (configurable)

/* Calculated constants based on audio configuration */
#define AUDIO_FRAME_SIZE        (AUDIO_CHANNELS * (AUDIO_BIT_DEPTH / 8))  // Bytes per frame
#define AUDIO_BUFFER_FRAMES     (512)   // Number of frames per buffer (configurable)
#define AUDIO_BUFFER_SIZE       (AUDIO_BUFFER_FRAMES * AUDIO_FRAME_SIZE)  // Total buffer size in bytes

/* DMA buffer management constants */
#define AUDIO_HALF_BUFFER_SIZE  (AUDIO_BUFFER_SIZE / 2)                   // Half buffer size in bytes
#define AUDIO_HALF_BUFFER_FRAMES (AUDIO_BUFFER_FRAMES / 2)                // Frames per half buffer
#define AUDIO_BUFFER_SAMPLES    (AUDIO_BUFFER_SIZE / 2)                   // Total 16-bit samples in buffer

/* SAI configuration constants */
#define SAI_FRAME_LENGTH        (AUDIO_CHANNELS * AUDIO_BIT_DEPTH)        // Total frame length in bits
#define SAI_SLOT_ACTIVE_MASK    ((1U << AUDIO_CHANNELS) - 1)             // Active slot mask based on channel count

/* Validation checks */
#if (AUDIO_CHANNELS != 4) && (AUDIO_CHANNELS != 8)
#error "AUDIO_CHANNELS must be either 4 or 8"
#endif

#if (AUDIO_BIT_DEPTH != 16)
#error "Only 16-bit audio is currently supported"
#endif

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
int audio_recorder_init(void);
int audio_recorder_reset(void);  // Force reset to clean state
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