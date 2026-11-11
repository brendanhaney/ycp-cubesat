#include "drivers/uart.h"
#include "utils/spi_demo.h"

int main(void) {

    uart_init();

    uart_printf("Hello from the Comms board!\n");

    spi_demo_slave("Radio", "RADIO>A", "A>RADIO");

    while (1);

    return 0;
}
