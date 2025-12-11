/**
 * @file spi_gpio_dispatcher.c
 * @brief SPI GPIO Event Dispatcher Implementation
 * @version 1.0
 * @date 2025-01-22
 */

#include "spi_gpio_dispatcher.h"
#include "spi_protocol_common.h"
#include "spi_protocol_media.h"
#include "spi_protocol_ota.h"
#include "shell_spictrl_port.h"
#include "gpio.h"
#include <string.h>

/* =================================================================== */
/* Private Types                                                      */
/* =================================================================== */

/* Message structure for GPIO events */
typedef struct {
    uint32_t event_type;     // Event type (GPIO_SPI_TRIGGER_EVENT, etc.)
    uint32_t timestamp;      // Event timestamp (for debugging)
} gpio_event_msg_t;

/* =================================================================== */
/* Private Variables                                                  */
/* =================================================================== */

/* Message queue for GPIO events */
static osMessageQueueId_t g_gpio_event_queue = NULL;

/* Export message queue for use by protocol modules */
osMessageQueueId_t gpio_spi_event_queue = NULL;

/* Dispatcher thread handle */
static osThreadId_t g_dispatcher_thread_id = NULL;

/* Enable/Disable flag */
static bool g_dispatcher_enabled = false;

/* SPI initialization state */
static bool g_spi_initialized = false;
gpio_config_t g_gpio_config = {
    .trigger_port = GPIOB,
    .trigger_pin = GPIO_PIN_12,   //2700 GPIO03
    .detect_port = GPIOB,
    .detect_pin = GPIO_PIN_9,    //2700 GPIO76
};

/* =================================================================== */
/* Forward Declarations                                               */
/* =================================================================== */

static void dispatcher_thread_entry(void *argument);
static int handle_photo_video_protocol(void);
static int handle_ota_firmware_transfer(void);

/* =================================================================== */
/* Public Functions                                                   */
/* =================================================================== */

int spi_gpio_dispatcher_init(void)
{
    if (g_gpio_event_queue != NULL) {
        TRACE_WARNING("GPIO Dispatcher: Already initialized");
        return 0;
    }
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable GPIOB clock
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // Configure PB12 (trigger pin) as external interrupt
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;  // Falling edge trigger
    GPIO_InitStruct.Pull = GPIO_PULLUP;           // Pull-up resistor
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // Configure PB6 (detect pin) as input
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // Input mode
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // No pull-up/down
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // Enable EXTI15_10 interrupt for PB12
    // Priority must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
    // to safely call FreeRTOS APIs from ISR
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    
    TRACE_INFO("GPIO Dispatcher: GPIO initialized - PB12(Trigger), PB6(Detect)");
    
    // Create message queue for GPIO events (depth: 10 messages)
    g_gpio_event_queue = osMessageQueueNew(10, sizeof(gpio_event_msg_t), NULL);
    if (g_gpio_event_queue == NULL) {
        TRACE_ERROR("GPIO Dispatcher: Failed to create message queue");
        return -1;
    }
    
    // Export for protocol modules
    gpio_spi_event_queue = g_gpio_event_queue;
    
    g_dispatcher_enabled = false;
    g_spi_initialized = false;
    
    TRACE_INFO("GPIO Dispatcher: Initialized successfully (Message Queue)");
    return 0;
}

int spi_gpio_dispatcher_start(void)
{
    if (g_gpio_event_queue == NULL) {
        TRACE_ERROR("GPIO Dispatcher: Not initialized, call spi_gpio_dispatcher_init() first");
        return -1;
    }
    
    if (g_dispatcher_thread_id != NULL) {
        TRACE_WARNING("GPIO Dispatcher: Thread already running");
        return 0;
    }
    
    // Thread attributes
    osThreadAttr_t thread_attr = {
        .name = "SPI_Dispatcher",
        .priority = SPI_DISPATCHER_THREAD_PRIORITY,
        .stack_size = SPI_DISPATCHER_THREAD_STACK_SIZE,
    };
    
    // Create dispatcher thread
    g_dispatcher_thread_id = osThreadNew(dispatcher_thread_entry, NULL, &thread_attr);
    if (g_dispatcher_thread_id == NULL) {
        TRACE_ERROR("GPIO Dispatcher: Failed to create thread");
        return -2;
    }
    
    TRACE_INFO("GPIO Dispatcher: Thread started successfully");
    return 0;
}

int spi_gpio_dispatcher_stop(void)
{
    if (g_dispatcher_thread_id == NULL) {
        return 0;
    }
    
    // Send stop message via queue
    gpio_event_msg_t msg;
    msg.event_type = GPIO_SPI_STOP_EVENT;
    msg.timestamp = osKernelGetTickCount();
    
    osMessageQueuePut(g_gpio_event_queue, &msg, 0, 100);
    
    // Wait for thread to terminate (with timeout)
    osDelay(200);
    
    g_dispatcher_thread_id = NULL;
    g_dispatcher_enabled = false;
    
    TRACE_INFO("GPIO Dispatcher: Thread stopped");
    return 0;
}

void spi_gpio_dispatcher_enable(bool enabled)
{
    g_dispatcher_enabled = enabled;
    TRACE_INFO("GPIO Dispatcher: %s", enabled ? "Enabled" : "Disabled");
}

bool spi_gpio_dispatcher_is_enabled(void)
{
    return g_dispatcher_enabled;
}

osMessageQueueId_t spi_gpio_dispatcher_get_event_queue(void)
{
    return g_gpio_event_queue;
}

void spi_gpio_dispatcher_set_trigger_pin(void *port, uint16_t pin)
{
    g_gpio_config.trigger_port = port;
    g_gpio_config.trigger_pin = pin;
    TRACE_INFO("GPIO Dispatcher: Trigger pin set to port=%p, pin=0x%04X", port, pin);
}

void spi_gpio_dispatcher_set_detect_pin(void *port, uint16_t pin)
{
    g_gpio_config.detect_port = port;
    g_gpio_config.detect_pin = pin;
    TRACE_INFO("GPIO Dispatcher: Detect pin set to port=%p, pin=0x%04X", port, pin);
}

void spi_gpio_dispatcher_irq_handler(uint16_t gpio_pin)
{
    if (g_gpio_event_queue == NULL) {
        return;
    }
    
    // Check if it's the trigger pin
    if (gpio_pin == g_gpio_config.trigger_pin) {
        // Read detect pin to determine business type
        GPIO_PinState detect_state = HAL_GPIO_ReadPin(
            (GPIO_TypeDef*)g_gpio_config.detect_port, 
            g_gpio_config.detect_pin
        );
        
        gpio_event_msg_t msg;
        msg.timestamp = osKernelGetTickCount();
        
        if (detect_state == GPIO_PIN_SET) {
            // High level: OTA firmware transfer mode
            msg.event_type = GPIO_SPI_UBOOT_DET_EVENT;
            TRACE_DEBUG("GPIO IRQ: OTA boot detected (GPIO high)");
        } else {
            // Low level: Normal business (Photo/Video/OTA upgrade)
            msg.event_type = GPIO_SPI_TRIGGER_EVENT;
            TRACE_DEBUG("GPIO IRQ: Normal trigger (GPIO low)");
        }
        
        // Send message to queue (non-blocking from ISR)
        osStatus_t status = osMessageQueuePut(g_gpio_event_queue, &msg, 0, 0);
        if (status == osOK) {
            TRACE_DEBUG("GPIO IRQ: Message sent, event=0x%02X, time=%lu", msg.event_type, msg.timestamp);
        } else {
            TRACE_ERROR("GPIO IRQ: Failed to send message, status=%d", status);
        }
    }
}

int spi_gpio_dispatcher_trigger_ota_transfer(void)
{
    if (g_gpio_event_queue == NULL) {
        TRACE_ERROR("GPIO Dispatcher: Not initialized");
        return -1;
    }
    
    if (g_dispatcher_thread_id == NULL) {
        TRACE_ERROR("GPIO Dispatcher: Thread not running");
        return -2;
    }
    
    // Send OTA boot event to trigger firmware transfer
    gpio_event_msg_t msg;
    msg.event_type = GPIO_SPI_UBOOT_DET_EVENT;
    msg.timestamp = osKernelGetTickCount();
    
    osStatus_t status = osMessageQueuePut(g_gpio_event_queue, &msg, 0, 100);
    if (status != osOK) {
        TRACE_ERROR("GPIO Dispatcher: Failed to send OTA transfer event, status=%d", status);
        return -3;
    }
    
    TRACE_INFO("GPIO Dispatcher: OTA firmware transfer event triggered");
    return 0;
}

/* =================================================================== */
/* Private Functions - Main Dispatcher Thread                        */
/* =================================================================== */

static void dispatcher_thread_entry(void *argument)
{
    (void)argument;
    int ret;
    gpio_event_msg_t msg;
    osStatus_t status;
    
    // Add a small delay to let shell return
    osDelay(100);
    
    TRACE_INFO("GPIO Dispatcher Thread: Started, waiting for messages...");
    TRACE_INFO("GPIO Dispatcher Thread: Message Queue = %p", g_gpio_event_queue);
    
    while (1) {
        // Wait for message from queue (blocking)
        TRACE_DEBUG("GPIO Dispatcher Thread: Waiting for message...");
        
        status = osMessageQueueGet(g_gpio_event_queue, &msg, NULL, osWaitForever);
        
        if (status != osOK) {
            TRACE_ERROR("GPIO Dispatcher Thread: Failed to receive message, status=%d", status);
            osDelay(10);
            continue;
        }
        
        TRACE_INFO("GPIO Dispatcher Thread: Received event=0x%02X, time=%lu", 
                   msg.event_type, msg.timestamp);
        
        // Check for stop event
        if (msg.event_type == GPIO_SPI_STOP_EVENT) {
            TRACE_INFO("GPIO Dispatcher Thread: Stop event received, exiting...");
            break;
        }
        
        // Check if dispatcher is enabled
        if (!g_dispatcher_enabled) {
            TRACE_WARNING("GPIO Dispatcher Thread: Event received but dispatcher disabled, event=0x%02X", 
                         msg.event_type);
            continue;
        }
        
        // Handle GPIO trigger events
        if (msg.event_type == GPIO_SPI_UBOOT_DET_EVENT || 
            msg.event_type == GPIO_SPI_TRIGGER_EVENT) {
            
            // Initialize SPI if needed
            if (!g_spi_initialized) {
                ret = spi_protocol_init();
                if (ret == 0) {
                    g_spi_initialized = true;
                    TRACE_INFO("GPIO Dispatcher: SPI initialized successfully");
                } else {
                    TRACE_ERROR("GPIO Dispatcher: SPI initialization failed, ret=%d", ret);
                    continue;
                }
            }
            
            // Dispatch based on event type
            if (msg.event_type == GPIO_SPI_UBOOT_DET_EVENT) {
                // OTA firmware transfer (device already in OTA boot mode)
                TRACE_INFO("GPIO Dispatcher: Dispatching to OTA firmware transfer...");
                ret = handle_ota_firmware_transfer();
                if (ret != 0) {
                    TRACE_ERROR("GPIO Dispatcher: OTA firmware transfer failed, ret=%d", ret);
                }
            } else if (msg.event_type == GPIO_SPI_TRIGGER_EVENT) {
                // Normal business - auto-detect (Photo/Video/OTA upgrade)
                TRACE_INFO("GPIO Dispatcher: Dispatching to auto business protocol...");
                ret = handle_photo_video_protocol();
                if (ret != 0) {
                    TRACE_ERROR("GPIO Dispatcher: Auto business protocol failed, ret=%d", ret);
                }
            }
        }
        
        // Handle state machine timeout events for OTA transfer
        if (msg.event_type == SPI_EVENT_TIMEOUT) {
            ota_protocol_state_t state = spi_protocol_ota_get_state();
            
            // Process state machine
            ret = spi_protocol_ota_state_machine_process();
            if (ret != 0) {
                TRACE_WARNING("GPIO Dispatcher: State machine processing error, ret=%d", ret);
            }
            
            // Check if transfer completed or failed
            ota_protocol_state_t new_state = spi_protocol_ota_get_state();
            if (new_state == OTA_STATE_TRANSFER_COMPLETE || 
                (state != OTA_STATE_IDLE && new_state == OTA_STATE_IDLE && ret != 0)) {
                    
                // Transfer completed or error occurred, re-enable GPIO interrupt
                HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
                TRACE_INFO("GPIO Dispatcher: OTA transfer finished, GPIO interrupt re-enabled");
                spi_protocol_ota_state_machine_init();
            }
        }
    }
    
    // Cleanup
    if (g_spi_initialized) {
        spi_protocol_deinit();
        g_spi_initialized = false;
    }
    
    TRACE_INFO("GPIO Dispatcher Thread: Exited");
    osThreadExit();
}

/* =================================================================== */
/* Private Functions - Protocol Handlers                             */
/* =================================================================== */

/**
 * @brief Handle Photo/Video auto-detection protocol
 * 
 * This function executes the auto business protocol which can detect
 * and handle Photo, Video, or OTA upgrade requests.
 */
static int handle_photo_video_protocol(void)
{
    int ret;
    
    // Use media auto-detect which handles Photo/Video/OTA upgrade detection
    ret = spi_protocol_media_auto_execute();
    
    if (ret == 0) {
        TRACE_INFO("GPIO Dispatcher: Auto business protocol completed successfully");
    } else {
        TRACE_WARNING("GPIO Dispatcher: Auto business protocol completed with code %d", ret);
    }
    
    return ret;
}

/**
 * @brief Handle OTA firmware transfer
 * 
 * This function is called when device is already in OTA boot mode.
 * It transfers the firmware package using state machine approach.
 */
static int handle_ota_firmware_transfer(void)
{
    int ret;
    
    TRACE_INFO("GPIO Dispatcher: Starting OTA firmware transfer (state machine mode)...");
    
    // Initialize state machine and let event loop handle the transfer
    ret = spi_protocol_ota_state_machine_init();
    
    if (ret == 0) {
        TRACE_INFO("GPIO Dispatcher: OTA state machine initialized, starting transfer...");
        
        // Disable GPIO interrupt during OTA transfer to avoid interruption
        HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
        TRACE_INFO("GPIO Dispatcher: GPIO interrupt disabled during OTA transfer");
        
        // Trigger first state machine step via message queue
        gpio_event_msg_t msg;
        msg.event_type = SPI_EVENT_TIMEOUT;
        msg.timestamp = osKernelGetTickCount();
        osMessageQueuePut(g_gpio_event_queue, &msg, 0, 100);
    } else {
        TRACE_ERROR("GPIO Dispatcher: Failed to initialize OTA state machine, ret=%d", ret);
    }
    
    return ret;
}
