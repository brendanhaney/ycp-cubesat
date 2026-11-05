#include <drivers.h>
#include <utils.h>
#include "utils/spi_demo.h"

int main(void) {

    uart_init();

    uart_printf("Hello from the Payload Camera!\n");

    if (delay_init(64000000U)) {
        uart_printf("Payload: delay initialization failed\n");
        return 1;
    }

    //spi_demo_slave("Payload", "PAYLD>A", "A>PAYLD");

    while (1) {

        uart_printf("Hello again\n");

        delay_ms(1000U);
    }

    return 0;
}
