#include <drivers.h>
#include "utils/spi_demo.h"

int main(void) {

    uart1_init();

    uart1_write_string("Hello from the Payload Camera!\n");

    spi_demo_slave("Payload", "PAYLD>A", "A>PAYLD");

    while (1);

    return 0;
}
