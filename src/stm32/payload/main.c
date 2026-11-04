#include <drivers.h>
#include "utils/spi_demo.h"

int main(void) {

    uart_init();

    uart_printf("Hello from the Payload Camera!\n");

    spi_demo_slave("Payload", "PAYLD>A", "A>PAYLD");

    while (1);

    return 0;
}
