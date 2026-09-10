#include <config.h>
#include <registers.h>
#include <drivers.h>

//#include "../../utils/crc.h"

int main(int argc, char **argv) {

    char *hello = "Avionics Computer";

    //uint32_t crc = crc32(0, hello, 18);

    uart1_init();

    uart1_write_string("Hello from the ");
    uart1_write_string(hello);
    uart1_write_string("!\n");

    while (1) {

    }

    return 0;
}