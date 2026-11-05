#include <config.h>

ENTRY(Reset_Handler)

MEMORY
{
#if defined(CUBESAT_STM32N6)
    STM32N6_SRAM (rwx) : ORIGIN = 0x34180400, LENGTH = 511K
#elif defined(CUBESAT_STM32H7)
    STM32H7_FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
    STM32H7_RAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
#endif
}

#if defined(CUBESAT_STM32N6)
REGION_ALIAS("CODE", STM32N6_SRAM);
REGION_ALIAS("RAM", STM32N6_SRAM);
#define IMAGE_SEGMENT :image
#define STACK_SEGMENT :stack
#define DATA_ALIGNMENT 32
#elif defined(CUBESAT_STM32H7)
REGION_ALIAS("CODE", STM32H7_FLASH);
REGION_ALIAS("RAM", STM32H7_RAM);
#define IMAGE_SEGMENT
#define STACK_SEGMENT
#define DATA_ALIGNMENT 4
#endif

_estack = ORIGIN(RAM) + LENGTH(RAM);
_sstack = _estack - 16K;

#if defined(CUBESAT_STM32N6)
PHDRS
{
    image PT_LOAD FLAGS(7);
    stack PT_LOAD FLAGS(6);
}
#endif

SECTIONS
{
    .isr_vector ORIGIN(CODE) : { KEEP(*(.isr_vector)) } > CODE IMAGE_SEGMENT
    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > CODE IMAGE_SEGMENT
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > CODE IMAGE_SEGMENT
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > CODE IMAGE_SEGMENT
    .preinit_array :
    {
        __preinit_array_start = .;
        KEEP(*(.preinit_array*))
        __preinit_array_end = .;
    } > CODE IMAGE_SEGMENT
    .init_array :
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > CODE IMAGE_SEGMENT
    .fini_array :
    {
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } > CODE IMAGE_SEGMENT
#if defined(CUBESAT_STM32N6)
    .gnu.sgstubs : { *(.gnu.sgstubs*) } > CODE IMAGE_SEGMENT
#endif
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(DATA_ALIGNMENT);
        _edata = .;
    } > RAM
#if defined(CUBESAT_STM32H7)
      AT > CODE
#endif
      IMAGE_SEGMENT
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > RAM IMAGE_SEGMENT
    .noinit (NOLOAD) : { *(.noinit .noinit.*) . = ALIGN(8); } > RAM IMAGE_SEGMENT
    _end = .;
    PROVIDE(end = _end);
    __HeapBase = _end;
    __HeapLimit = _sstack;
    .stack _sstack (NOLOAD) : { . += 16K; } > RAM STACK_SEGMENT
    .ARM.attributes 0 : { *(.ARM.attributes) }
    /DISCARD/ : { *(.note.GNU-stack) }
}

ASSERT((vector_table & 0x3ff) == 0, "vector table alignment")
#if defined(CUBESAT_STM32N6)
ASSERT(SIZEOF(.isr_vector) == 844, "STM32N657 vector table size")
ASSERT((_edata & 31) == 0, "FSBL payload alignment")
ASSERT(__HeapBase + 2K <= __HeapLimit, "AXISRAM2 exhausted by image, heap and stack")
#elif defined(CUBESAT_STM32H7)
ASSERT(SIZEOF(.isr_vector) == 664, "STM32H753 vector table size")
ASSERT(__HeapBase + 2K <= __HeapLimit, "RAM exhausted by data, heap and stack")
#endif
