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
        uart1_write_string("SPI init failed\n");
        return;
    }
    for (unsigned int i = 0; i < SPI_DEMO_ROUNDS; i++) {
        if (spi4_slave_transfer(reply, received, sizeof(received)) ||
            spi_demo_check(received, expected)) {
            uart1_write_string("SPI exchange failed\n");
            return;
        }
        uart1_write_string(name);
        uart1_write_string(" SPI RX: ");
        uart1_write_string(received);
        uart1_write_string("; TX: ");
        uart1_write_string(reply);
        uart1_write_string("\n");
    }
    uart1_write_string("SPI demo complete\n");
}
