#ifndef SPI_MODEL_H
#define SPI_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define SPI_MODEL_BASE 0x40013400U
#define SPI_MODEL_SIZE 0x400U
#define SPI_MODEL_FIFO_SIZE 8U

// Host-side peripheral state, separate from the STM32 firmware driver.
struct spi_fifo {
    uint8_t bytes[SPI_MODEL_FIFO_SIZE];
    unsigned int head, count;
};

struct spi_slave {
    uint32_t registers[SPI_MODEL_SIZE / 4];
    struct spi_fifo tx, rx;
    uint32_t count, flags;
    unsigned int bytes_transferred, transactions, errors;
    bool selected;
};

struct spi_bus {
    struct spi_slave *slaves[2];
    bool selected[2];
    unsigned int bytes_transferred, selection_errors;
};

void spi_model_reset(struct spi_slave *slave);
void spi_model_select(struct spi_slave *slave, bool selected);
uint32_t spi_model_read(struct spi_slave *slave, unsigned int offset);
void spi_model_write(struct spi_slave *slave, unsigned int offset, uint32_t value);
uint8_t spi_model_transmit(struct spi_slave *slave, uint8_t value);
void spi_bus_select(struct spi_bus *bus, unsigned int index, bool selected);
uint8_t spi_bus_transmit(struct spi_bus *bus, uint8_t value);

#endif
