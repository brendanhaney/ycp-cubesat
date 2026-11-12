#include "drivers/uart.h"
#include "utils/delay.h"
#include <payload_spi.h>
#include <string.h>

volatile unsigned int debug_spi_requests;
volatile unsigned int debug_spi_errors;

int main(void) {

    uart_init();

    uart_printf("Hello from the Comms board!\n");

    /* Startup keeps the H753 core on its 64 MHz HSI clock. */
    if (delay_init(64000000U) || payload_spi_init()) {
        uart_printf("Comms SPI init failed\n");
        return 1;
    }

    uart_printf("Comms SPI master: requesting payload string\n");

    while (1) {
        char reply[PAYLOAD_SPI_SIZE];

        if (payload_spi_request_string(reply)) {
            debug_spi_errors++;
            uart_printf("Comms SPI: payload not ready or transfer failed; retrying\n");
        } else if (memcmp(reply, payload_spi_reply, sizeof(reply))) {
            debug_spi_errors++;
            uart_printf("Comms SPI: payload busy or invalid reply; retrying\n");
        } else {
            debug_spi_requests++;
            uart_printf("Comms SPI RX: ");
            uart_write_str(reply);
            uart_printf("\n");
        }

        delay_ms(1000);
    }
}
