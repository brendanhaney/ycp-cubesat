#include <config.h>
#include <drivers.h>
#include "utils/spi_demo.h"

int main(void) {

    const char messages[2][SPI_DEMO_SIZE] = {"A>RADIO", "A>PAYLD"};
    const char replies[2][SPI_DEMO_SIZE] = {"RADIO>A", "PAYLD>A"};
    char received[SPI_DEMO_SIZE];

    uart1_init();

    uart1_write_string("Hello from the Avionics Computer!\n");

    if (spi4_init(1)) {

        uart1_write_string("SPI init failed\n");

        return 1;
    }

    for (unsigned int i = 0; i < SPI_DEMO_ROUNDS; i++) {

        for (unsigned int slave = SPI_RADIO; slave <= SPI_PAYLOAD; slave++) {

            if (spi4_master_transfer(slave, messages[slave], received, sizeof(received)) ||
                spi_demo_check(received, replies[slave])) {

                uart1_write_string("SPI exchange failed\n");

                return 1;
            }

            uart1_write_string("Avionics SPI RX: ");
            uart1_write_string(received);
            uart1_write_string("; TX: ");
            uart1_write_string(messages[slave]);
            uart1_write_string("\n");
        }
    }

    uart1_write_string("SPI demo complete\n");

    while (1);

    return 0;
}
