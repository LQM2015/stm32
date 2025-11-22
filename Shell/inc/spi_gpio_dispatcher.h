/**
 * @file spi_gpio_dispatcher.h
 * @brief SPI GPIO Event Dispatcher - Central Event Handler
 * @version 1.0
 * @date 2025-01-22
 * 
 * @description
 * This module provides a centralized GPIO event dispatcher thread that:
 * - Monitors GPIO interrupt events
 * - Dispatches events to appropriate protocol handlers
 * - Manages SPI initialization
 * - Supports auto-detection of business types (Photo/Video/OTA)
 */

#ifndef __SPI_GPIO_DISPATCHER_H__
#define __SPI_GPIO_DISPATCHER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

/* =================================================================== */
/* Event Flags Definitions                                            */
/* =================================================================== */

#define GPIO_SPI_TRIGGER_EVENT       (1 << 0)  /*!< General SPI trigger event */
#define GPIO_SPI_STOP_EVENT          (1 << 1)  /*!< Stop dispatcher thread */
#define GPIO_SPI_UBOOT_DET_EVENT     (1 << 2)  /*!< OTA boot mode detected */
#define SPI_EVENT_TIMEOUT            (1 << 3)  /*!< Timeout event for state machine */

/* =================================================================== */
/* Dispatcher Configuration                                           */
/* =================================================================== */

/**
 * @brief Dispatcher thread priority
 */
#ifndef SPI_DISPATCHER_THREAD_PRIORITY
#define SPI_DISPATCHER_THREAD_PRIORITY  osPriorityLow
#endif

/**
 * @brief Dispatcher thread stack size
 */
#ifndef SPI_DISPATCHER_THREAD_STACK_SIZE
#define SPI_DISPATCHER_THREAD_STACK_SIZE  8192
#endif

/* GPIO pin configuration */
typedef struct {
    void *trigger_port;      // Trigger pin port (e.g., GPIOB)
    uint16_t trigger_pin;    // Trigger pin number (e.g., GPIO_PIN_12)
    void *detect_port;       // Detection pin port (e.g., GPIOB)
    uint16_t detect_pin;     // Detection pin number (e.g., GPIO_PIN_6)
} gpio_config_t;

extern gpio_config_t g_gpio_config;
/* =================================================================== */
/* Public Functions                                                   */
/* =================================================================== */

/**
 * @brief Initialize GPIO event dispatcher
 * 
 * Creates event flags and initializes the dispatcher system.
 * Must be called before starting the dispatcher thread.
 * 
 * @return 0 on success, negative on error
 */
int spi_gpio_dispatcher_init(void);

/**
 * @brief Start GPIO event dispatcher thread
 * 
 * Starts the main event dispatcher thread that monitors GPIO events
 * and dispatches to appropriate protocol handlers.
 * 
 * @return 0 on success, negative on error
 */
int spi_gpio_dispatcher_start(void);

/**
 * @brief Stop GPIO event dispatcher thread
 * 
 * Gracefully stops the dispatcher thread and cleans up resources.
 * 
 * @return 0 on success, negative on error
 */
int spi_gpio_dispatcher_stop(void);

/**
 * @brief Enable/Disable auto protocol handling
 * 
 * @param enabled true to enable auto handling, false to disable
 */
void spi_gpio_dispatcher_enable(bool enabled);

/**
 * @brief Check if dispatcher is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool spi_gpio_dispatcher_is_enabled(void);

/**
 * @brief Get message queue handle
 * 
 * Returns the message queue handle for external event triggering
 * (e.g., from timer callbacks or other modules)
 * 
 * @return Message queue handle, or NULL if not initialized
 */
osMessageQueueId_t spi_gpio_dispatcher_get_event_queue(void);

/**
 * @brief Set GPIO pin for trigger detection
 * 
 * @param port GPIO port (e.g., GPIOB)
 * @param pin GPIO pin number (e.g., GPIO_PIN_12)
 */
void spi_gpio_dispatcher_set_trigger_pin(void *port, uint16_t pin);

/**
 * @brief Set GPIO pin for OTA mode detection
 * 
 * @param port GPIO port (e.g., GPIOB)
 * @param pin GPIO pin number (e.g., GPIO_PIN_6)
 */
void spi_gpio_dispatcher_set_detect_pin(void *port, uint16_t pin);

/**
 * @brief GPIO interrupt callback (to be called from HAL_GPIO_EXTI_Callback)
 * 
 * This function should be called from your GPIO interrupt handler to
 * trigger the dispatcher event processing.
 * 
 * @param gpio_pin The GPIO pin that triggered the interrupt
 * 
 * @example
 * void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 * {
 *     spi_gpio_dispatcher_irq_handler(GPIO_Pin);
 * }
 */
void spi_gpio_dispatcher_irq_handler(uint16_t gpio_pin);

/**
 * @brief Manually trigger OTA firmware transfer state machine
 * 
 * This function can be called from shell commands or other modules
 * to directly enter the OTA firmware transfer state machine.
 * The dispatcher thread must be running for this to work.
 * 
 * @return 0 on success, negative on error
 */
int spi_gpio_dispatcher_trigger_ota_transfer(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_GPIO_DISPATCHER_H__ */
