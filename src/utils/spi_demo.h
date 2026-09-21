#ifndef SPI_DEMO_H
#define SPI_DEMO_H

// Fixed-size demonstration frames include their terminating NUL.
#define SPI_DEMO_SIZE 8U
void spi_demo_slave(const char *name, const char *reply, const char *expected);
int spi_demo_check(const char *received, const char *expected);

#endif
