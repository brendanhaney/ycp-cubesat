#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <registers.h>

#if defined(CUBESAT_STM32N6)
/* STM32CubeN6 system_stm32n6xx_fsbl.c; RM0486 and ES0620. */
#define SYSTICK_CTRL STM32N6_REG32(0xE000E010U)
#define SCB_ICSR STM32N6_REG32(0xE000ED04U)
#define NVIC_ICER(bank) STM32N6_REG32(0xE000E180U + (bank) * 4U)
#define NVIC_ICPR(bank) STM32N6_REG32(0xE000E280U + (bank) * 4U)
#define SAU_CTRL STM32N6_REG32(0xE000EDD0U)
#define SAU_RNR STM32N6_REG32(0xE000EDD8U)
#define SAU_RBAR STM32N6_REG32(0xE000EDDCU)
#define SAU_RLAR STM32N6_REG32(0xE000EDE0U)

extern const uint32_t vector_table[];

void SystemInit(void) {
    SYSTICK_CTRL = 0;
    SCB_ICSR = (1U << 25) | (1U << 27);
    for (unsigned int bank = 0; bank < 7; ++bank) {
        NVIC_ICER(bank) = 0xFFFFFFFFU;
        NVIC_ICPR(bank) = 0xFFFFFFFFU;
    }

    SAU_CTRL = 0;
    for (unsigned int region = 0; region < 8; ++region) {
        SAU_RNR = region;
        SAU_RBAR = 0;
        SAU_RLAR = 0;
    }

    STM32N6_RCC_AHB3RSTSR = 1U;
    STM32N6_RCC_AHB3RSTCR = 1U;
    STM32N6_RCC_AHB3ENCR = 1U;

    STM32N6_RCC_AHB4ENSR = 1U << 18;
    (void)STM32N6_RCC_AHB4ENR;
    /* ST-LINK hot-plug needs SYSCFG to remain clocked. */
    STM32N6_RCC_APB4ENSR2 = 1U;
    (void)STM32N6_RCC_APB4ENR2;
    STM32N6_SYSCFG_INITSVTORCR = (uint32_t)vector_table;

    STM32N6_PWR_SVMCR1 |= 1U << 8;
    STM32N6_PWR_SVMCR2 |= 1U << 8;
    STM32N6_PWR_SVMCR3 |= (1U << 8) | (1U << 9);
    STM32N6_SYSCFG_VDDIO4CCCR = 0x287U;
    STM32N6_SYSCFG_VDDIO5CCCR = 0x287U;
    STM32N6_SYSCFG_VDDIO2CCCR = 0x287U;
    STM32N6_SYSCFG_VDDIO3CCCR = 0x287U;
    STM32N6_SYSCFG_VDDCCCR = 0x287U;
    STM32N6_PWR_SVMCR3 |= (1U << 12) | (1U << 4);
    (void)STM32N6_PWR_SVMCR3;
    STM32N6_RCC_APB4ENR1 |= 1U << 15;

    STM32N6_RCC_APB4ENR2 |= 1U << 4;
    (void)STM32N6_RCC_APB4ENR2;
    STM32N6_RCC_APB4ENR2 &= ~(1U << 4);
    STM32N6_RCC_AHB5RSTSR = (1U << 12) | (1U << 13);
    STM32N6_RCC_AHB5RSTCR = (1U << 12) | (1U << 13);
    STM32N6_RCC_APB1RSTSR1 = 1U;
    STM32N6_RCC_APB1RSTCR1 = 1U;
    STM32N6_RCC_APB1ENCR1 = 1U;
    STM32N6_RCC_AHB4ENCR = 1U << 6;
    (void)STM32N6_SYSCFG_INITSVTORCR;
    __asm volatile ("dsb\nisb" ::: "memory");
}

#elif defined(CUBESAT_STM32H7)
void SystemInit(void) {}
#endif

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
extern char __HeapBase, __HeapLimit;
extern void __libc_init_array(void);
extern int main(void);

void _init(void) {}

void *_sbrk(ptrdiff_t increment) {
    static uintptr_t current;
    const uintptr_t base = (uintptr_t)&__HeapBase;
    const uintptr_t limit = (uintptr_t)&__HeapLimit;

    if (!current)
        current = base;

    uintptr_t previous = current;
    if (increment >= 0) {
        if ((uintptr_t)increment > limit - current)
            goto exhausted;
        current += (uintptr_t)increment;
    } else {
        uintptr_t decrease = (uintptr_t)(-(increment + 1)) + 1U;
        if (decrease > current - base)
            goto exhausted;
        current -= decrease;
    }
    return (void *)previous;

exhausted:
    errno = ENOMEM;
    return (void *)-1;
}

__attribute__((noreturn)) void runtime_start(void) {
    const uint32_t *source = &_sidata;
    volatile uint32_t *destination = &_sdata;
    while (destination < &_edata)
        *destination++ = *source++;

    destination = &_sbss;
    while (destination < &_ebss)
        *destination++ = 0;

    __libc_init_array();
    __asm volatile ("cpsie i" ::: "memory");
    (void)main();
    for (;;) {}
}
