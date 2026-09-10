#include <typedefs.h>
#include <registers.h>

#include "uart.h"

void uart1_init(void) {
    // Enable USART peripheral and transmitter
    USART_CR1 |= USART_CR1_UE | USART_CR1_TE;
}

void uart1_write_char(char c) {
    // Wait until transmit register is empty
    while (!(USART_ISR & USART_ISR_TXE));
    USART_TDR = (uint32_t)c;
}

void uart1_write_string(const char *str) {
    while (*str) {
        uart1_write_char(*str++);
    }
}