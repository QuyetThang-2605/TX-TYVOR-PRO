/* STM32F407VE: 512 KiB flash, 112+16 KiB contiguous SRAM, 64 KiB CCM.
   All DMA/UART data and main stack use SRAM, never CCM. No bootloader offset. */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 4096;
SECTIONS {
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; _end = .; end = .; } > RAM
  .ccmram (NOLOAD) : { . = ALIGN(4); *(.ccmram*) . = ALIGN(4); } > CCMRAM
  .stack_reserve (NOLOAD) : { . = ALIGN(8); . += _Min_Stack_Size; . = ALIGN(8); } > RAM
  /DISCARD/ : { *(.note.GNU-stack) }
  .ARM.attributes 0 : { *(.ARM.attributes) }
}
ASSERT(_ebss + _Min_Stack_Size <= _estack, "SRAM exhausted: static data overlaps reserved stack")
