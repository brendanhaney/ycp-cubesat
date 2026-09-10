#include "typedefs.h"

// Linker script symbols
extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata;
extern uint32_t _sbss, _ebss;

extern int main(void);

// The STM32 expects 'Reset_Handler' to be the main entry point of the program.
// Cortex-M hardware alse expects a mandatory table mapped directly at memory address 
// 0x00000000. This driver ensures these parameters.

void Reset_Handler(void) {

    // Copy initialized data from Flash to RAM
    uint32_t *pSrc = &_sidata;
    uint32_t *pDest = &_sdata;

    while (pDest < &_edata) {

        *pDest++ = *pSrc++;

    }

    // Zero out BSS section in RAM
    pDest = &_sbss;

    while (pDest < &_ebss) {

        *pDest++ = 0;

    }

    // Jump to main
    main();

    while (1);
}

// Map vector table to the .isr_vector section defined in stm32.ld
__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uint32_t) &_estack,
    (uint32_t) &Reset_Handler
};