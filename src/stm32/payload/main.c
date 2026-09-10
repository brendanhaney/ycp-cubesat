#include <config.h>
#include <drivers.h>

int main(int argc, char **argv) {

    char *hello = "Payload Camera";

    uart1_init();

    uart1_write_string("Hello from the ");
    uart1_write_string(hello);
    uart1_write_string("!\n");

    while (1) {

    }

    return 0;
}