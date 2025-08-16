#ifndef __CLOCK_MANAGEMENT_H
#define __CLOCK_MANAGEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief Enumeration for system clock performance profiles.
  */
typedef enum
{
  CLOCK_PROFILE_32K,    /*!< 32 kHz, ultra low power mode (LSI) */
  CLOCK_PROFILE_24M,    /*!< 24 MHz, low power mode (HSI/4) */
  CLOCK_PROFILE_48M,    /*!< 48 MHz, energy saving mode */
  CLOCK_PROFILE_96M,    /*!< 96 MHz, balanced mode */
  CLOCK_PROFILE_128M,   /*!< 128 MHz, standard mode */
  CLOCK_PROFILE_200M,   /*!< 200 MHz, high efficiency mode */
  CLOCK_PROFILE_300M,   /*!< 300 MHz, high performance mode */
  CLOCK_PROFILE_400M,   /*!< 400 MHz, ultra high performance mode */
  CLOCK_PROFILE_550M    /*!< 550 MHz, maximum performance mode */
} ClockProfile_t;

HAL_StatusTypeDef SwitchSystemClock(ClockProfile_t profile);
uint32_t GetCurrentSystemClock(void);
ClockProfile_t GetCurrentClockProfile(void);
void TestAllClockProfiles(void);

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_MANAGEMENT_H */