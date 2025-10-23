/**
 * @file shell_spictrl_port.h
 * @brief Platform adaptation layer for shell_spictrl.c
 * @version 1.0
 * @date 2025-01-22
 * 
 * This header provides platform-specific adaptations for the SPI control
 * shell command, mapping BES platform APIs to STM32 HAL APIs.
 */

#ifndef __SHELL_SPICTRL_PORT_H__
#define __SHELL_SPICTRL_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"
#include "gpio.h"
#include "ff.h"
#include "shell.h"
#include "shell_log.h"
#include "cmsis_os2.h"

/* =================================================================== */
/* SPI Interface Adaptation                                           */
/* =================================================================== */

#define SPI_TIMEOUT_MS  1000

/**
 * @brief Initialize SPI peripheral
 * @return 0 on success, negative on error
 */
static inline int platform_spi_init(void)
{
    MX_SPI1_Init();
    return 0;
}

/**
 * @brief Deinitialize SPI peripheral
 * @return 0 on success, negative on error
 */
static inline int platform_spi_deinit(void)
{
    HAL_SPI_DeInit(&hspi1);
    return 0;
}

/**
 * @brief Transmit data via SPI
 * @param data Pointer to data buffer
 * @param size Number of bytes to transmit
 * @return 0 on success, negative on error
 */
static inline int platform_spi_transmit(const uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, (uint8_t*)data, size, SPI_TIMEOUT_MS);
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief Receive data via SPI
 * @param data Pointer to receive buffer
 * @param size Number of bytes to receive
 * @return 0 on success, negative on error
 */
static inline int platform_spi_receive(uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi1, data, size, SPI_TIMEOUT_MS);
    return (status == HAL_OK) ? 0 : -1;
}

/**
 * @brief Transmit and receive data via SPI (full-duplex)
 * @param tx_data Pointer to transmit buffer
 * @param rx_data Pointer to receive buffer
 * @param size Number of bytes to transfer
 * @return 0 on success, negative on error
 */
static inline int platform_spi_transmit_receive(const uint8_t *tx_data, 
                                                 uint8_t *rx_data, 
                                                 uint16_t size)
{
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, 
                                                        (uint8_t*)tx_data, 
                                                        rx_data, 
                                                        size, 
                                                        SPI_TIMEOUT_MS);
    return (status == HAL_OK) ? 0 : -1;
}

/* =================================================================== */
/* GPIO Interface Adaptation                                          */
/* =================================================================== */

/* GPIO Pin Definitions - adjust according to your hardware */
#define GPIO_PORT_TRIGGER    GPIOB
#define GPIO_PIN_TRIGGER     GPIO_PIN_12

#define GPIO_PORT_DETECT     GPIOB
#define GPIO_PIN_DETECT      GPIO_PIN_6

/**
 * @brief Read GPIO pin state
 * @param port GPIO port
 * @param pin GPIO pin number
 * @return Pin state (0 or 1)
 */
static inline int platform_gpio_read(GPIO_TypeDef *port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1 : 0;
}

/**
 * @brief Write GPIO pin state
 * @param port GPIO port
 * @param pin GPIO pin number
 * @param value Pin state to set (0 or 1)
 */
static inline void platform_gpio_write(GPIO_TypeDef *port, uint16_t pin, int value)
{
    HAL_GPIO_WritePin(port, pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* =================================================================== */
/* File System Interface Adaptation                                   */
/* =================================================================== */

/* File handle type adaptation */
typedef FIL platform_file_t;

/**
 * @brief Open file
 * @param file Pointer to file object
 * @param path File path
 * @param mode Access mode (FA_READ, FA_WRITE, etc.)
 * @return 0 on success, negative on error
 */
static inline int platform_file_open(platform_file_t *file, const char *path, BYTE mode)
{
    FRESULT res = f_open(file, path, mode);
    return (res == FR_OK) ? 0 : -((int)res);
}

/**
 * @brief Close file
 * @param file Pointer to file object
 * @return 0 on success, negative on error
 */
static inline int platform_file_close(platform_file_t *file)
{
    FRESULT res = f_close(file);
    return (res == FR_OK) ? 0 : -((int)res);
}

/**
 * @brief Read from file
 * @param file Pointer to file object
 * @param buffer Pointer to buffer
 * @param size Number of bytes to read
 * @param bytes_read Pointer to store actual bytes read
 * @return 0 on success, negative on error
 */
static inline int platform_file_read(platform_file_t *file, void *buffer, 
                                      size_t size, size_t *bytes_read)
{
    UINT br;
    FRESULT res = f_read(file, buffer, size, &br);
    if (bytes_read) *bytes_read = br;
    return (res == FR_OK) ? 0 : -((int)res);
}

/**
 * @brief Seek file position
 * @param file Pointer to file object
 * @param offset Offset from beginning of file
 * @return 0 on success, negative on error
 */
static inline int platform_file_seek(platform_file_t *file, size_t offset)
{
    FRESULT res = f_lseek(file, offset);
    return (res == FR_OK) ? 0 : -((int)res);
}

/**
 * @brief Get file information
 * @param path File path
 * @param fsize Pointer to store file size
 * @return 0 on success, negative on error
 */
static inline int platform_file_stat(const char *path, size_t *fsize)
{
    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    if (fsize) *fsize = fno.fsize;
    return (res == FR_OK) ? 0 : -((int)res);
}

/**
 * @brief Check if file exists
 * @param path File path
 * @return 1 if exists, 0 if not exists
 */
static inline int platform_file_exists(const char *path)
{
    FILINFO fno;
    return (f_stat(path, &fno) == FR_OK) ? 1 : 0;
}

/* =================================================================== */
/* Debug Print Adaptation                                            */
/* =================================================================== */

/* Map TRACE macros to SHELL_LOG */
#define TRACE_INFO(fmt, ...)    SHELL_LOG_USER_INFO(fmt, ##__VA_ARGS__)
#define TRACE_ERROR(fmt, ...)   SHELL_LOG_USER_ERROR(fmt, ##__VA_ARGS__)
#define TRACE_WARNING(fmt, ...) SHELL_LOG_USER_WARNING(fmt, ##__VA_ARGS__)
#define TRACE_DEBUG(fmt, ...)   SHELL_LOG_USER_DEBUG(fmt, ##__VA_ARGS__)

/* Generic TRACE macro - maps to INFO level */
#define TRACE(level, fmt, ...)  SHELL_LOG_USER_INFO(fmt, ##__VA_ARGS__)

/* Hex dump function */
static inline void DUMP8_IMPL(const char *fmt, const uint8_t *data, size_t len)
{
    Shell *shell = shellGetCurrent();
    if (shell && data) {
        for (size_t i = 0; i < len; i++) {
            shellPrint(shell, fmt, data[i]);
        }
        shellPrint(shell, "\r\n");
    }
}
#define DUMP8(fmt, data, len) DUMP8_IMPL(fmt, data, len)

/* =================================================================== */
/* Shell Command Adaptation                                           */
/* =================================================================== */

/**
 * @brief Get current shell instance
 * @return Pointer to shell instance
 */
static inline Shell* platform_shell_get_current(void)
{
    return shellGetCurrent();
}

/**
 * @brief Print string to shell
 * @param shell Shell instance
 * @param str String to print
 */
static inline void platform_shell_print(Shell *shell, const char *str)
{
    if (shell && str) {
        shellPrint(shell, "%s", str);
    }
}

/**
 * @brief Print formatted string to shell
 * @param shell Shell instance
 * @param fmt Format string
 * @param ... Arguments
 */
#define platform_shell_printf(shell, fmt, ...) \
    do { \
        if (shell) { \
            shellPrint(shell, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/* =================================================================== */
/* Memory Allocation Adaptation                                       */
/* =================================================================== */

/**
 * @brief Allocate memory from heap
 * @param size Size in bytes
 * @return Pointer to allocated memory, NULL on failure
 */
static inline void* platform_malloc(size_t size)
{
    return pvPortMalloc(size);
}

/**
 * @brief Free allocated memory
 * @param ptr Pointer to memory to free
 */
static inline void platform_free(void *ptr)
{
    if (ptr) {
        vPortFree(ptr);
    }
}

/* =================================================================== */
/* Utility Functions                                                  */
/* =================================================================== */

/**
 * @brief Get system tick count in milliseconds
 * @return Tick count
 */
static inline uint32_t platform_get_tick_ms(void)
{
    return HAL_GetTick();
}

/**
 * @brief Delay in milliseconds
 * @param ms Delay time in milliseconds
 */
static inline void platform_delay_ms(uint32_t ms)
{
    osDelay(ms);
}

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_SPICTRL_PORT_H__ */
