/**
  ******************************************************************************
  * @file           : fault_handler.h
  * @brief          : STM32H7 Fault Handler and Exception Dump Module
  * @description    : Comprehensive fault information capture for debugging
  *                   HardFault, MemManage, BusFault, UsageFault exceptions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef __FAULT_HANDLER_H
#define __FAULT_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

/* Fault type definitions */
#define FAULT_TYPE_HARDFAULT    0
#define FAULT_TYPE_MEMMANAGE    1
#define FAULT_TYPE_BUSFAULT     2
#define FAULT_TYPE_USAGEFAULT   3
#define FAULT_TYPE_NMI          4

/* Stack frame offset definitions (in words) */
#define STACK_FRAME_R0          0
#define STACK_FRAME_R1          1
#define STACK_FRAME_R2          2
#define STACK_FRAME_R3          3
#define STACK_FRAME_R12         4
#define STACK_FRAME_LR          5
#define STACK_FRAME_PC          6
#define STACK_FRAME_XPSR        7

/* Extended stack frame for FPU (8 additional words for S0-S15, FPSCR) */
#define STACK_FRAME_SIZE_BASIC  8
#define STACK_FRAME_SIZE_FPU    26

/* Exported types ------------------------------------------------------------*/

/**
 * @brief CPU register context structure
 */
typedef struct {
    /* Core registers from stack frame */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;        /* Link Register */
    uint32_t pc;        /* Program Counter (fault address) */
    uint32_t xpsr;      /* Program Status Register */
    
    /* Additional registers saved manually */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    
    /* Stack pointers */
    uint32_t msp;       /* Main Stack Pointer */
    uint32_t psp;       /* Process Stack Pointer */
    
    /* Special registers */
    uint32_t control;   /* CONTROL register */
    uint32_t basepri;   /* BASEPRI register */
    uint32_t primask;   /* PRIMASK register */
    uint32_t faultmask; /* FAULTMASK register */
    
    /* FPU registers (if FPU is enabled) */
    uint32_t fpscr;     /* FPU Status and Control Register */
} FaultContext_t;

/**
 * @brief Fault Status Register information
 */
typedef struct {
    /* Hard Fault Status Register (HFSR) */
    uint32_t hfsr;
    uint8_t  hfsr_forced;       /* Forced HardFault */
    uint8_t  hfsr_vecttbl;      /* Vector table read fault */
    
    /* Configurable Fault Status Register (CFSR) */
    uint32_t cfsr;
    
    /* MemManage Fault Status Register (MMFSR - part of CFSR) */
    uint8_t  mmfsr_mmarvalid;   /* MMFAR valid */
    uint8_t  mmfsr_mstkerr;     /* Stacking error */
    uint8_t  mmfsr_munstkerr;   /* Unstacking error */
    uint8_t  mmfsr_daccviol;    /* Data access violation */
    uint8_t  mmfsr_iaccviol;    /* Instruction access violation */
    
    /* BusFault Status Register (BFSR - part of CFSR) */
    uint8_t  bfsr_bfarvalid;    /* BFAR valid */
    uint8_t  bfsr_stkerr;       /* Stacking error */
    uint8_t  bfsr_unstkerr;     /* Unstacking error */
    uint8_t  bfsr_impreciserr;  /* Imprecise data access error */
    uint8_t  bfsr_preciserr;    /* Precise data access error */
    uint8_t  bfsr_ibuserr;      /* Instruction bus error */
    
    /* UsageFault Status Register (UFSR - part of CFSR) */
    uint8_t  ufsr_divbyzero;    /* Divide by zero */
    uint8_t  ufsr_unaligned;    /* Unaligned access */
    uint8_t  ufsr_nocp;         /* No coprocessor */
    uint8_t  ufsr_invpc;        /* Invalid PC */
    uint8_t  ufsr_invstate;     /* Invalid state */
    uint8_t  ufsr_undefinstr;   /* Undefined instruction */
    
    /* Fault Address Registers */
    uint32_t mmfar;             /* MemManage Fault Address Register */
    uint32_t bfar;              /* BusFault Address Register */
    
    /* Auxiliary Fault Status Register */
    uint32_t afsr;
    
    /* Debug Fault Status Register */
    uint32_t dfsr;
} FaultStatus_t;

/**
 * @brief Complete fault information structure
 */
typedef struct {
    uint32_t fault_type;        /* Type of fault */
    uint32_t exc_return;        /* EXC_RETURN value */
    uint32_t active_stack;      /* 0 = MSP, 1 = PSP */
    FaultContext_t context;     /* CPU context */
    FaultStatus_t status;       /* Fault status registers */
    uint32_t timestamp;         /* System tick at fault time */
    char task_name[16];         /* Current FreeRTOS task name (if applicable) */
} FaultInfo_t;

/* Exported variables --------------------------------------------------------*/
extern volatile FaultInfo_t g_fault_info;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Initialize fault handler module
 */
void FaultHandler_Init(void);

/**
 * @brief Main fault handler entry point (called from exception handlers)
 * @param stack_ptr Pointer to the stack frame
 * @param exc_return EXC_RETURN value from LR
 * @param fault_type Type of fault (FAULT_TYPE_xxx)
 */
void FaultHandler_Process(uint32_t *stack_ptr, uint32_t exc_return, uint32_t fault_type);

/**
 * @brief Dump fault information to console via shell log
 * @param info Pointer to fault information structure
 */
void FaultHandler_DumpInfo(const FaultInfo_t *info);

/**
 * @brief Dump stack memory around fault address
 * @param stack_ptr Stack pointer
 * @param depth Number of words to dump
 */
void FaultHandler_DumpStack(uint32_t *stack_ptr, uint32_t depth);

/**
 * @brief Dump memory region
 * @param addr Start address
 * @param length Number of bytes to dump
 */
void FaultHandler_DumpMemory(uint32_t addr, uint32_t length);

/**
 * @brief Get fault type string
 * @param fault_type Fault type
 * @return Fault type string
 */
const char* FaultHandler_GetFaultTypeString(uint32_t fault_type);

/**
 * @brief Print fault analysis and possible causes
 * @param info Pointer to fault information structure
 */
void FaultHandler_AnalyzeFault(const FaultInfo_t *info);

/**
 * @brief Low-level print function that works even in fault context
 * @param format Format string
 * @param ... Arguments
 */
void FaultHandler_Printf(const char *format, ...);

/**
 * @brief Dump all FreeRTOS task information
 */
void FaultHandler_DumpTasks(void);

/**
 * @brief Dump FreeRTOS software timer information
 */
void FaultHandler_DumpTimers(void);

/**
 * @brief Dump possible call stack backtrace
 * @param stack_ptr Stack pointer
 * @param pc Program counter at fault
 * @param lr Link register at fault
 */
void FaultHandler_DumpBacktrace(uint32_t *stack_ptr, uint32_t pc, uint32_t lr);

/**
 * @brief Dump current task's stack usage
 * @param task_handle Task handle (NULL for current task)
 */
void FaultHandler_DumpTaskStack(void *task_handle);

/* Assembly wrapper functions declarations */
void HardFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return);
void MemManage_Handler_C(uint32_t *stack_ptr, uint32_t exc_return);
void BusFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return);
void UsageFault_Handler_C(uint32_t *stack_ptr, uint32_t exc_return);

#ifdef __cplusplus
}
#endif

#endif /* __FAULT_HANDLER_H */
