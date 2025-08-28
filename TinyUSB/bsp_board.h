/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef BSP_BOARD_H_
#define BSP_BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "stm32h7xx_hal.h"

// Common LPC and MIMXRT MCUs
#if TU_CHECK_MCU(OPT_MCU_LPC11UXX, OPT_MCU_LPC13XX, OPT_MCU_LPC15XX, OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC18XX, OPT_MCU_LPC40XX, OPT_MCU_LPC43XX, OPT_MCU_MCXN9XX, OPT_MCU_MIMXRT) || \
   (CFG_TUSB_MCU == OPT_MCU_LPC51UXX) || (CFG_TUSB_MCU == OPT_MCU_LPC54XXX) || (CFG_TUSB_MCU == OPT_MCU_LPC55XX)
  static inline uint32_t board_millis(void)
  {
    return HAL_GetTick();
  }

#elif CFG_TUSB_MCU == OPT_MCU_ESP32S2 || CFG_TUSB_MCU == OPT_MCU_ESP32S3
  static inline uint32_t board_millis(void)
  {
    return HAL_GetTick();
  }

#elif CFG_TUSB_MCU == OPT_MCU_RP2040
  static inline uint32_t board_millis(void)
  {
    return HAL_GetTick();
  }

#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  static inline uint32_t board_millis(void)
  {
    return HAL_GetTick();
  }

#else
  uint32_t board_millis(void);
#endif

//--------------------------------------------------------------------+
// Board Porting API
//--------------------------------------------------------------------+

// Initialize board IO, USB, LEDs, buttons
void board_init(void);

// Turn LED on or off
void board_led_write(bool state);

// Get the current state of button
// a '1' means active (pressed), a '0' means inactive.
uint32_t board_button_read(void);

// Get characters from UART
int board_uart_read(uint8_t* buf, int len);

// Send characters to UART
int board_uart_write(void const * buf, int len);

#ifdef __cplusplus
 }
#endif

#endif
