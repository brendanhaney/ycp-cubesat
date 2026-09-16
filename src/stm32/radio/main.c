#include <drivers.h>
#include "utils/spi_demo.h"

int main(void) {

    uart1_init();

    uart1_write_string("Hello from the Comms System!\n");

    spi_demo_slave("Radio", "RADIO>A", "A>RADIO");

    while (1);

    return 0;
}
