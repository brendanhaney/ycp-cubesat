#include <typedefs.h>
#include <config.h>
#include <registers.h>

#include <stdio.h>
#include <stdarg.h>

#include "uart.h"
#if defined(CUBESAT_STM32N6)
#include "gpio.h"
#endif

void uart_init(void) {
#if defined(CUBESAT_STM32H7)
#ifdef CUBESAT_NUCLEO_H753ZI
    STM32H7_RCC_CR |= 1U;
    while (!(STM32H7_RCC_CR & (1U << 2)));
    STM32H7_RCC_D2CCIP2R = (STM32H7_RCC_D2CCIP2R & ~7U) | 3U;
    STM32H7_RCC_AHB4ENR |= 1U << 3;
    STM32H7_RCC_APB1LENR |= 1U << 18;
    (void)STM32H7_RCC_AHB4ENR;
    (void)STM32H7_RCC_APB1LENR;

    STM32H7_RCC_APB1LRSTR |= 1U << 18;
    STM32H7_RCC_APB1LRSTR &= ~(1U << 18);

    GPIO_AFRH(STM32H7_GPIOD_BASE) = (GPIO_AFRH(STM32H7_GPIOD_BASE) & ~0xFFU) | 0x77U;
    GPIO_OTYPER(STM32H7_GPIOD_BASE) &= ~0x300U;
    GPIO_OSPEEDR(STM32H7_GPIOD_BASE) = (GPIO_OSPEEDR(STM32H7_GPIOD_BASE) & ~0xF0000U) | 0x50000U;
    GPIO_PUPDR(STM32H7_GPIOD_BASE) &= ~0xF0000U;
    GPIO_MODER(STM32H7_GPIOD_BASE) = (GPIO_MODER(STM32H7_GPIOD_BASE) & ~0xF0000U) | 0xA0000U;

    USART_CR1 = 0;
    USART_CR2 = 0;
    USART_CR3 = 0;
    USART_PRESC = 0;
    const uint32_t kernel_hz = 64000000U >> ((STM32H7_RCC_CR >> 3) & 3U);
    USART_BRR = (kernel_hz + 115200U / 2U) / 115200U;
    USART_CR1 = USART_CR1_UE | USART_CR1_TE;
#else
    USART_CR1 |= USART_CR1_UE | USART_CR1_TE;
#endif
#elif defined(CUBESAT_STM32N6)
    uint32_t remaining = SPI_POLL_LIMIT;
    STM32N6_RCC_CR |= 1U << 3;
    for (; remaining; remaining--) {
        if (STM32N6_RCC_SR & (1U << 3))
            break;
    }
    if (!remaining)
        return;
    STM32N6_RCC_BUSENSR = (1U << 6) | (1U << 9);
    (void)STM32N6_RCC_BUSENR;
    STM32N6_RCC_AHB4ENSR = (1U << 18) | (1U << 4) | (1U << 1);
    (void)STM32N6_RCC_AHB4ENR;
    STM32N6_PWR_SVMCR3 &= ~(1U << 24);
    STM32N6_RCC_CCIPR13 = (STM32N6_RCC_CCIPR13 & ~7U) | 6U;
    STM32N6_RCC_APB2ENSR = 1U << 4;
    (void)STM32N6_RCC_APB2ENR;
    STM32N6_RCC_APB2RSTSR = 1U << 4;
    STM32N6_RCC_APB2RSTCR = 1U << 4;

    gpio_alternate(GPIOE_BASE, 5U, 7U, 0U);
    gpio_alternate(GPIOE_BASE, 6U, 7U, 0U);
    USART_CR1 = 0;
    USART_CR2 = 0;
    USART_CR3 = 0;
    USART_PRESC = 0;
    uint32_t kernel_hz = 64000000U >> ((STM32N6_RCC_HSICFGR >> 7) & 3U);
    USART_BRR = (kernel_hz + 115200U / 2U) / 115200U;
    USART_CR1 = USART_CR1_UE | USART_CR1_TE;
#endif
}

void uart_write_byte(byte_t b) {

    // Wait until transmit register is empty
    while (!(USART_ISR & USART_ISR_TXE)) {};

    USART_TDR = (uint32_t) b;

}


void uart_write_str(const char *str) {

    while (*str) {

        uart_write_byte(*str++);

    }

}

void uart_printf(const char *fmt, ...) {

    va_list args;
    va_start(args, fmt);

    char buff[32];

    for (const char *p = fmt; *p != '\0'; p++) {

        if (*p == '%') {

            p++; 

            switch (*p) {

                case 'd':
                    snprintf(buff, sizeof(buff), "%d", va_arg(args, int));
                    uart_write_str(buff);
                    break;

                case 'f':
                    snprintf(buff, sizeof(buff), "%f", va_arg(args, double));
                    uart_write_str(buff);
                    break;

                case 'x':
                    snprintf(buff, sizeof(buff), "%x", va_arg(args, unsigned int));
                    uart_write_str(buff);
                    break;

                default:
                    uart_write_byte('%');
                    uart_write_byte(*p);
                    break;
            }

        } else {

            uart_write_byte(*p);

        }
    }

    va_end(args);

}
