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
  CLOCK_PROFILE_HIGH_PERF,  /*!< 550 MHz, for maximum performance */
  CLOCK_PROFILE_POWER_SAVE  /*!< 200 MHz, for balanced power saving */
} ClockProfile_t;

void SwitchSystemClock(ClockProfile_t profile);

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_MANAGEMENT_H */