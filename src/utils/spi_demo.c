#include <config.h>
#include <drivers.h>

#include "spi_demo.h"

int spi_demo_check(const char *received, const char *expected) {
    for (unsigned int i = 0; i < SPI_DEMO_SIZE; i++) {
        if (received[i] != expected[i]) {
            return -1;
        }
    }
    return 0;
}

void spi_demo_slave(const char *name, const char *reply, const char *expected) {
    char received[SPI_DEMO_SIZE];

    if (spi4_init(0)) {
        uart_printf("SPI init failed\n");
        return;
    }
    for (unsigned int i = 0; i < SPI_DEMO_ROUNDS; i++) {
        if (spi4_slave_transfer(reply, received, sizeof(received)) ||
            spi_demo_check(received, expected)) {
            uart_printf("SPI exchange failed\n");
            return;
        }
        uart_printf(name);
        uart_printf(" SPI RX: ");
        uart_printf(received);
        uart_printf("; TX: ");
        uart_printf(reply);
        uart_printf("\n");
    }
    uart_printf("SPI demo complete\n");
}
