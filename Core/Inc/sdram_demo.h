/**
 * @file    sdram_demo.h
 * @brief   SDRAM execution demonstration functions
 * @note    These functions are placed in external SDRAM for execution
 */

#ifndef __SDRAM_DEMO_H
#define __SDRAM_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Functions placed in SDRAM */
void SDRAM_DemoFunction(void);
void SDRAM_DemoPrintBanner(void);

/* External SDRAM constant */
extern const char sdram_demo_banner[];

#ifdef __cplusplus
}
#endif

#endif /* __SDRAM_DEMO_H */
