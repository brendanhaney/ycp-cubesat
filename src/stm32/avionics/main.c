#include <config.h>
#include <registers.h>

#include "../../utils/crc.h"

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

int main(int argc, char **argv) {

    char *hello = "Avionics Computer";

    uint32_t crc = crc32(0, hello, 18);

    uart1_init();

    uart1_write_string("Hello from the ");
    uart1_write_string(hello);
    uart1_write_string("!\n");

    while (1) {

    }

    return 0;
}