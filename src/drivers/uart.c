#include <typedefs.h>
#include <registers.h>

#include <stdio.h>
#include <stdarg.h>

#include "uart.h"

void uart_init(void) {

#ifdef CUBESAT_NUCLEO_H753ZI
    // UM2407: the ST-LINK VCP is connected to USART3 on PD8/PD9, AF7.
    RCC_CR |= 1U; // Enable HSI, keeping the existing system clock/divider.
    while (!(RCC_CR & (1U << 2)));
    RCC_D2CCIP2R = (RCC_D2CCIP2R & ~7U) | 3U; // USART234578 kernel = HSI.
    RCC_AHB4ENR |= 1U << 3; // GPIOD
    RCC_APB1LENR |= 1U << 18; // USART3
    (void)RCC_AHB4ENR;
    (void)RCC_APB1LENR;

    // Reset only this USART so framing doesn't depend on a previous program.
    RCC_APB1LRSTR |= 1U << 18;
    RCC_APB1LRSTR &= ~(1U << 18);

    // PD8=TX, PD9=RX: alternate function, push-pull, medium speed, no pulls.
    GPIO_AFRH(GPIOD_BASE) = (GPIO_AFRH(GPIOD_BASE) & ~0xFFU) | 0x77U;
    GPIO_OTYPER(GPIOD_BASE) &= ~0x300U;
    GPIO_OSPEEDR(GPIOD_BASE) = (GPIO_OSPEEDR(GPIOD_BASE) & ~0xF0000U) | 0x50000U;
    GPIO_PUPDR(GPIOD_BASE) &= ~0xF0000U;
    GPIO_MODER(GPIOD_BASE) = (GPIO_MODER(GPIOD_BASE) & ~0xF0000U) | 0xA0000U;

    USART_CR1 = 0; // Configure while disabled: 8 data bits, no parity, oversample 16.
    USART_CR2 = 0; // One stop bit.
    USART_CR3 = 0; // No hardware flow control.
    USART_PRESC = 0; // Kernel clock /1.
    const uint32_t kernel_hz = 64000000U >> ((RCC_CR >> 3) & 3U);
    USART_BRR = (kernel_hz + 115200U / 2U) / 115200U;
    USART_CR1 = USART_CR1_UE | USART_CR1_TE;
#else

    // Enable USART peripheral and transmitter
    USART_CR1 |= USART_CR1_UE | USART_CR1_TE;

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