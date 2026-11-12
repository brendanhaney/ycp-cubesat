#ifndef PAYLOAD_SPI_H
#define PAYLOAD_SPI_H

#include <stdint.h>
#include <image_protocol.h>

/* Fixed-size, mode-0 transfers on Arduino D10..D13, without READY. */
#define PAYLOAD_SPI_SIZE IMAGE_SPI_FRAME_SIZE

/* H753 SPI1 prescaler encoding: 5 = 1 MHz, 4 = 2 MHz, 3 = 4 MHz
 * with the existing 64 MHz HSI source. Does not change any RCC clocks. */
#ifndef PAYLOAD_SPI_MASTER_MBR
#define PAYLOAD_SPI_MASTER_MBR 3U
#endif
#if PAYLOAD_SPI_MASTER_MBR > 7 || PAYLOAD_SPI_MASTER_MBR < 0
#error "PAYLOAD_SPI_MASTER_MBR must be between 0 and 7"
#endif
#define PAYLOAD_SPI_MASTER_CLOCK_HZ (64000000U / (2U << PAYLOAD_SPI_MASTER_MBR))

/* H753: SPI1 master. N657: SPI5 slave. */
int payload_spi_init(void);
/* H753 only: clocks one complete frame; returns 0 or -1 on failure. */
int payload_spi_exchange(const uint8_t tx[PAYLOAD_SPI_SIZE],
                         uint8_t rx[PAYLOAD_SPI_SIZE]);
/* N657 only: returns 1 for a complete frame after CS rises, 0 if idle,
 * or -1 for a short frame/peripheral error. Never waits for the master. */
int payload_spi_service(uint8_t request[PAYLOAD_SPI_SIZE]);
/* N657 only: replace the reply for the next exchange while paused. */
void payload_spi_set_response(const uint8_t response[PAYLOAD_SPI_SIZE]);
/* N657 only: discard partial work before camera capture or SD access. */
void payload_spi_pause(void);

#endif
