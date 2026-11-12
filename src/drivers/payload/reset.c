#include <stdint.h>

#include "stm32n6xx.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
extern void __libc_init_array(void);
extern int main(void);

/* A warm SRAM load replaces the previous application. Enter from privileged
 * Thread mode, with the loader having invalidated the instruction cache after
 * downloading the image. The previous application's mutable data is uncached. */
__attribute__((used, noinline, noreturn)) static void payload_reset_runtime(void) {
    ARM_MPU_Disable();
    SCB_InvalidateICache();
    SCB_InvalidateDCache();

    /* Capture survives a debugger halt. Hold the receiver and DMA pipeline in
     * reset before startup changes their clocks or reinitializes memory.
     * The camera MSP releases both resets after configuring the new clocks. */
    RCC->APB5RSTSR = RCC_APB5RSTSR_CSIRSTS | RCC_APB5RSTSR_DCMIPPRSTS;
    __DSB();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    for (uint32_t bank = 0; bank < 7; bank++) {
        NVIC->ICER[bank] = UINT32_MAX;
        NVIC->ICPR[bank] = UINT32_MAX;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    __DSB();
    __ISB();

    SystemInit();

    const uint32_t *source = &_sidata;
    volatile uint32_t *destination = &_sdata;
    while (destination < &_edata) {
        *destination++ = *source++;
    }
    destination = &_sbss;
    while (destination < &_ebss) {
        *destination++ = 0;
    }

    __libc_init_array();
    (void)main();
    for (;;) {
        __WFI();
    }
}

/* Override the vendor's weak entry point while retaining its vector table. */
__attribute__((naked, noreturn)) void Reset_Handler(void) {
    __asm volatile(
        "cpsid i\n"
        /* Disable both caches before any literal-pool or C data accesses. */
        "movw r0, #0xed00\n"
        "movt r0, #0xe000\n"
        "ldr r1, [r0, #0x14]\n"
        "bic r1, r1, #0x30000\n"
        "str r1, [r0, #0x14]\n"
        "dsb\n"
        "isb\n"
        "movs r0, #0\n"
        "msr msplim, r0\n"
        "msr psplim, r0\n"
        "msr control, r0\n"
        "isb\n"
        "msr basepri, r0\n"
        "msr faultmask, r0\n"
        "msr psp, r0\n"
        "ldr r0, =_estack\n"
        "msr msp, r0\n"
        "ldr r0, =_sstack\n"
        "msr msplim, r0\n"
        "b payload_reset_runtime\n"
    );
}
