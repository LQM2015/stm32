/**
 ******************************************************************************
 * @file      STM32H750_FlashLoader.ld
 * @brief     Linker script for STM32H750 External Loader
 *            Device: STM32H750XBHx
 *            128Kbytes RAM
 * 
 * @attention
 * This linker script is specifically for STM32CubeProgrammer External Loaders
 * The .dev_info section MUST be at VirtAddr 0x00000000 in its own LOAD segment!
 ******************************************************************************
 */

/* Entry Point */
ENTRY(Init)

/* Heap and Stack sizes */
_Min_Heap_Size = 0x200;      /* 512 bytes minimum heap */
_Min_Stack_Size = 0x400;     /* 1024 bytes minimum stack */

/* Memory Regions for External Loader */
MEMORY
{
  /* Special region for StorageInfo - VirtAddr must be 0 for STM32CubeProgrammer */
  INFO (r)   : ORIGIN = 0x00000000, LENGTH = 1K
  
  /* RAM region for code and data - using AXI SRAM
   * IMPORTANT: Start at 0x24000004 (not 0x24000000) to match IAR convention
   * STM32CubeProgrammer expects this 4-byte offset!
   */
  RAM (xrw)  : ORIGIN = 0x24000004, LENGTH = 512K - 4
}

/* End of RAM - used by _sbrk */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Sections */
SECTIONS
{
  /* Device info structure - MUST be at address 0 in its own segment */
  /* This is a special requirement of STM32CubeProgrammer */
  .dev_info 0x00000000 :
  {
    KEEP(*(.dev_info))
  } > INFO AT> INFO
  
  /* Code section - in RAM */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > RAM

  /* Read-only data */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > RAM

  /* ARM exception handling */
  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > RAM
  
  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > RAM

  /* Preinit array */
  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > RAM

  /* Init array */
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > RAM

  /* Fini array */
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > RAM

  /* Initialized data */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM

  /* Uninitialized data */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } > RAM

  /* Heap and Stack */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + 0x200;      /* Minimal heap */
    . = . + 0x400;      /* Minimal stack */
    . = ALIGN(8);
  } > RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
