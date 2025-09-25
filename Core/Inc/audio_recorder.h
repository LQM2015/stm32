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

typedef enum {
    AUDIO_MODE_I2S_STEREO = 0,
    AUDIO_MODE_I2S_TDM,
    AUDIO_MODE_COUNT
} AudioPcmMode_t;

typedef struct {
    AudioPcmMode_t mode;           /**< Profile identifier */
    const char *name;              /**< Human readable profile name */
    uint8_t channels;              /**< Number of active audio channels */
    uint8_t bit_depth;             /**< Bits per sample (currently 16) */
    uint32_t sample_rate;          /**< Sample rate in Hz */
    uint32_t buffer_frames;        /**< Frames per DMA buffer */
    uint32_t sai_protocol;         /**< Protocol passed to HAL_SAI_InitProtocol */
    uint32_t sai_datasize;         /**< Datasize passed to HAL_SAI_InitProtocol */
    uint32_t slot_active_mask;     /**< Active slot mask for multi-slot protocols */
} AudioPcmConfig_t;

typedef struct {
    uint8_t channels;
    uint8_t bit_depth;
    uint32_t sample_rate;
    AudioRecorderState_t state;
    char filename[64];
    uint32_t bytes_written;
    uint32_t buffer_size;
    bool file_open;
    bool write_in_progress;  /**< Flag to prevent reentrant writes */
} __attribute__((aligned(32))) AudioRecorder_t;  /**< 32-byte aligned to match cache line */

// 独立的文件对象，不与录音器结构体混在一起
// extern FIL g_audio_file __attribute__((section(".dma_buffer")));  // 放在DMA安全区域
// 改为在源文件中定义为static，不需要extern声明

/* Exported constants --------------------------------------------------------*/
#define AUDIO_SUPPORTED_BIT_DEPTH      16U
#define AUDIO_MAX_CHANNELS             8U
#define AUDIO_DMA_BUFFER_FRAMES        512U

#define AUDIO_DMA_FRAME_BYTES_MAX      (AUDIO_MAX_CHANNELS * (AUDIO_SUPPORTED_BIT_DEPTH / 8U))
#define AUDIO_DMA_BUFFER_SIZE_MAX      (AUDIO_DMA_BUFFER_FRAMES * AUDIO_DMA_FRAME_BYTES_MAX)
#define AUDIO_DMA_HALF_BUFFER_SIZE_MAX (AUDIO_DMA_BUFFER_SIZE_MAX / 2U)
#define AUDIO_DMA_BUFFER_SAMPLES_MAX   (AUDIO_DMA_BUFFER_SIZE_MAX / 2U)
#define AUDIO_DMA_HALF_BUFFER_SAMPLES  (AUDIO_DMA_BUFFER_SAMPLES_MAX / 2U)

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
void audio_recorder_measure_clock(void);  // Measure external clock frequency

/* Runtime configuration helpers */
AudioPcmMode_t audio_recorder_get_mode(void);
HAL_StatusTypeDef audio_recorder_set_mode(AudioPcmMode_t mode);
const AudioPcmConfig_t* audio_recorder_get_pcm_config(void);
const AudioPcmConfig_t* audio_recorder_get_pcm_config_for_mode(AudioPcmMode_t mode);
uint32_t audio_recorder_get_channel_count(void);
uint32_t audio_recorder_get_sample_rate(void);
uint32_t audio_recorder_get_bit_depth(void);
uint32_t audio_recorder_get_bytes_per_frame(void);
uint32_t audio_recorder_get_total_buffer_bytes(void);
uint32_t audio_recorder_get_half_buffer_bytes(void);
uint32_t audio_recorder_get_total_buffer_samples(void);
uint32_t audio_recorder_get_half_buffer_samples(void);
uint32_t audio_recorder_get_slot_active_mask(void);
uint32_t audio_recorder_get_sai_protocol(void);
uint32_t audio_recorder_get_sai_datasize(void);

/* Callback functions */
void audio_recorder_rx_complete_callback(void);
void audio_recorder_rx_half_complete_callback(void);
void audio_recorder_error_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_RECORDER_H__ */