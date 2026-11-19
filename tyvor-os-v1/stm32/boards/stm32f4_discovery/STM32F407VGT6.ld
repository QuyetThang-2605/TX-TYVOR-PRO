/* MB997E Discovery / STM32F407VG. No bootloader offset, no heap/output driver.
 * SRAM1+SRAM2 are contiguous 128 KiB; CCM64 KiB is a distinct region. */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 8192;
__StackLimit = _estack - _Min_Stack_Size;
SECTIONS {
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) KEEP(*(.tyvor_identity)) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } > FLASH
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
ASSERT(_ebss <= __StackLimit, "Discovery SRAM exhausted: static data overlaps main stack budget")
ASSERT(SIZEOF(.isr_vector) == 392, "Expected 98 STM32F407 vector entries")
