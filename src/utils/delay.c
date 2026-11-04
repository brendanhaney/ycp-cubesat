#include "delay.h"

// Arm Cortex-M7 SysTick
#define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010U)
#define SYSTICK_LOAD (*(volatile uint32_t *)0xE000E014U)
#define SYSTICK_VAL  (*(volatile uint32_t *)0xE000E018U)

#define CTRL_ENABLE    (1U << 0)
#define CTRL_CLKSOURCE (1U << 2)
#define CTRL_COUNTFLAG (1U << 16)
#define RELOAD_MAX     0xFFFFFFU

int delay_init(uint32_t freq) {

    uint32_t ticks = freq / 1000U;

    if (!ticks || freq % 1000U || ticks - 1U > RELOAD_MAX)
        return -1;

    SYSTICK_CTRL = 0;
    SYSTICK_LOAD = ticks - 1U;
    SYSTICK_VAL = 0;

    SYSTICK_CTRL = CTRL_CLKSOURCE | CTRL_ENABLE;

    return 0;
}

void delay_ms(uint32_t ms) {

    if (!ms)
        return;

    // Tick count needs to be reset on delay
    SYSTICK_VAL = 0;

    while (ms--) {

        while (!(SYSTICK_CTRL & CTRL_COUNTFLAG)) {}

    }

}
