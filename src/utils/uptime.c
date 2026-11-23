#include "uptime.h"
#include <config.h>
#include <registers.h>

#if defined(CUBESAT_STM32H7)
#define TIMER_REG(offset) MMIO32(0x40000000U + (offset))

int uptime_init(uint32_t timer_clock_hz) {

    const uint32_t divider = timer_clock_hz / 1000U;

    if (!divider || divider > 65536U || timer_clock_hz % 1000U)
        return -1;

    STM32H7_RCC_APB1LENR |= 1U;

    (void)STM32H7_RCC_APB1LENR;

    STM32H7_RCC_APB1LRSTR |= 1U;

    STM32H7_RCC_APB1LRSTR &= ~1U;

    TIMER_REG(0x00U) = 0;
    TIMER_REG(0x28U) = divider - 1U;
    TIMER_REG(0x2CU) = 0xFFFFFFFFU;
    TIMER_REG(0x24U) = 0;
    TIMER_REG(0x14U) = 1U;
    TIMER_REG(0x10U) = 0;
    TIMER_REG(0x00U) = 1U;

    return 0;
}

uint32_t uptime_ms(void) {

    return TIMER_REG(0x24U);

}
#endif
