#ifndef PAYLOAD_SPI_DMA_H
#define PAYLOAD_SPI_DMA_H

#include <stdint.h>

int payload_spi_dma_init(void);
int payload_spi_dma_start(const uint8_t *tx, uint8_t *rx, uint32_t length);

int payload_spi_dma_status(void);
void payload_spi_dma_stop(void);

void payload_spi_dma_complete(int error);

#endif
