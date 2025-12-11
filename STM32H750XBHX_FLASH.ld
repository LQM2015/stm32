/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7 series
**                128Kbytes FLASH and 1056Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2025 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

/* Specify the memory areas for STM32H750XBH6 - APP Configuration */
MEMORY
{
  /* 外部QSPI Flash作为主程序存储区域 */
  FLASH (rx)       : ORIGIN = 0x90000000, LENGTH = 32M       /* QSPI Flash映射地址 */
  /* STM32H750XBH6的RAM布局 */
  DTCMRAM (xrw)    : ORIGIN = 0x20000000, LENGTH = 128K      /* DTCM @ 0x20000000 */
  RAM_D1 (xrw)     : ORIGIN = 0x24000000, LENGTH = 512K      /* AXI SRAM @ 0x24000000 */
  RAM_D2 (xrw)     : ORIGIN = 0x30000000, LENGTH = 288K      /* SRAM1+SRAM2 @ 0x30000000 */
  RAM_D3 (xrw)     : ORIGIN = 0x38000000, LENGTH = 64K       /* SRAM4 @ 0x38000000 */
  ITCMRAM (xrw)    : ORIGIN = 0x00000000, LENGTH = 64K       /* ITCM @ 0x00000000 */
  SDRAM   (rx)     : ORIGIN = 0xC0000000, LENGTH = 32M       /* External SDRAM @ 0xC0000000 */
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    /* Only match .text.xxx (with dot), not .text_xxx (with underscore) */
    *(.text.*)
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    /* Only match .rodata.xxx (with dot), not .rodata_xxx (with underscore) */
    *(.rodata.*)
    
    /* Shell command section - for Letter Shell command registration */
    . = ALIGN(4);
    _shell_command_start = .;
    KEEP (*(shellCommand))
    _shell_command_end = .;
    
    . = ALIGN(4);
  } >FLASH

  /* SDRAM code and constants (loaded from FLASH, run in SDRAM) */
  /* Place functions or consts into these sections by using section attributes */
  /* e.g., __attribute__((section(".text_sdram"))) */
  __sdram_text_load = LOADADDR(.text_sdram);
  .text_sdram :
  {
    . = ALIGN(4);
    __sdram_text_start = .;
    *(.text_sdram)
    *(.text_sdram*)
    . = ALIGN(4);
    __sdram_text_end = .;
  } >SDRAM AT> FLASH

  __sdram_rodata_load = LOADADDR(.rodata_sdram);
  .rodata_sdram :
  {
    . = ALIGN(4);
    __sdram_rodata_start = .;
    *(.rodata_sdram)
    *(.rodata_sdram*)
    . = ALIGN(4);
    __sdram_rodata_end = .;
  } >SDRAM AT> FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH
  
  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM_D1 AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM_D1

  /* DMA accessible memory in D2 domain (required for DMA1/DMA2 on STM32H7) */
  .RAM_D2 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RAM_D2)
    *(.RAM_D2*)
    . = ALIGN(4);
  } >RAM_D2

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1


  .ARM.attributes 0 : { *(.ARM.attributes) }
}


