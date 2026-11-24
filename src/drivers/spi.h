#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <config.h>
#include <typedefs.h>

#if SPI_DEMO_MASTER_MBR < 0 || SPI_DEMO_MASTER_MBR > 7
#error "SPI master prescaler encoding must be between 0 and 7"
#endif
#if SPI_POLL_LIMIT < 1 || SPI_DEMO_ROUNDS < 1
#error "SPI poll limits and demo rounds must be nonzero"
#endif

#define SPI_RADIO   0U
#define SPI_PAYLOAD 1U
// SPI4 has an 8-byte FIFO. Preload whole transactions so polling never
// depends on the slave CPU keeping up with SCK. Payloads are arbitrary bytes.
#define SPI_MAX_TRANSFER 8U

int spi4_init(int master);
// Blocking full-duplex transactions. Both endpoints must agree on len (1..8).
// Return 0 on success, -1 on invalid arguments, timeout or peripheral error.
int spi4_master_transfer(unsigned int slave, const void *tx, void *rx, size_t len);
// Waits indefinitely for selection; bounds waits once a transaction starts.
int spi4_slave_transfer(const void *tx, void *rx, size_t len);

#endif
