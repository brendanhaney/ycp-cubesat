#include <string.h>

#include "spi_model.h"

static void fault(struct spi_slave *slave, uint32_t flag) {
    slave->flags |= flag;
    slave->errors++;
}

static void push(struct spi_fifo *fifo, uint8_t value) {
    fifo->bytes[(fifo->head + fifo->count) % SPI_MODEL_FIFO_SIZE] = value;
    fifo->count++;
}

static uint8_t pop(struct spi_fifo *fifo) {
    uint8_t value = fifo->bytes[fifo->head];
    fifo->head = (fifo->head + 1) % SPI_MODEL_FIFO_SIZE;
    fifo->count--;
    return value;
}

void spi_model_reset(struct spi_slave *slave) {
    memset(slave, 0, sizeof(*slave));
}

void spi_model_select(struct spi_slave *slave, bool selected) {
    if (slave->selected && !selected && slave->count) {
        slave->transactions++;
        if (slave->count != (slave->registers[1] & 0xFFFFU)) {
            fault(slave, 1U << 8);
        }
    }
    slave->selected = selected;
}

uint8_t spi_model_transmit(struct spi_slave *slave, uint8_t value) {
    uint8_t result = 0;

    if (!slave->selected || !(slave->registers[0] & 1U)) {
        fault(slave, 1U << 8);
        return 0xFF;
    }
    // Mode 0, MSB first, 8-bit, hardware NSS, full duplex only.
    if ((slave->registers[2] & 31U) != 7U ||
        (slave->registers[3] & 0x17FE0000U)) {
        fault(slave, 1U << 8);
        return 0xFF;
    }
    if (slave->count >= (slave->registers[1] & 0xFFFFU)) {
        fault(slave, 1U << 6);
        return 0xFF;
    }
    if (!slave->tx.count) {
        fault(slave, 1U << 5);
    } else {
        result = pop(&slave->tx);
    }
    if (slave->rx.count == SPI_MODEL_FIFO_SIZE) {
        fault(slave, 1U << 6);
    } else {
        push(&slave->rx, value);
    }
    slave->count++;
    slave->bytes_transferred++;
    if (slave->count == (slave->registers[1] & 0xFFFFU)) {
        slave->flags |= 1U << 3;
    }
    return result;
}

uint32_t spi_model_read(struct spi_slave *slave, unsigned int offset) {
    if (offset >= SPI_MODEL_SIZE || offset % 4) {
        fault(slave, 1U << 8);
        return 0;
    }
    if (offset == 0x14) {
        return slave->flags | (slave->rx.count ? 1U : 0U) |
            (slave->tx.count < SPI_MODEL_FIFO_SIZE ? 2U : 0U) |
            ((slave->flags & 8U) ? 4096U : 0U);
    }
    if (offset == 0x30) {
        if (!slave->rx.count) {
            fault(slave, 1U << 6);
            return 0;
        }
        return pop(&slave->rx);
    }
    return slave->registers[offset / 4];
}

void spi_model_write(struct spi_slave *slave, unsigned int offset, uint32_t value) {
    if (offset >= SPI_MODEL_SIZE || offset % 4) {
        fault(slave, 1U << 8);
        return;
    }
    if (offset == 0x20) {
        if (!(slave->registers[0] & 1U) || slave->tx.count == SPI_MODEL_FIFO_SIZE) {
            fault(slave, 1U << 5);
        } else {
            push(&slave->tx, (uint8_t)value);
        }
        return;
    }
    if (offset == 0x18) {
        slave->flags &= ~value;
        return;
    }
    if ((offset == 8 || offset == 12) && (slave->registers[0] & 1U)) {
        fault(slave, 1U << 8);
        return;
    }
    slave->registers[offset / 4] = value;
    if (offset == 0 && !(value & 1U)) {
        slave->tx = (struct spi_fifo){0};
        slave->rx = (struct spi_fifo){0};
        slave->count = slave->flags = 0;
    }
}

void spi_bus_select(struct spi_bus *bus, unsigned int index, bool selected) {
    if (index > 1) {
        bus->selection_errors++;
        return;
    }
    bus->selected[index] = selected;
    if (bus->slaves[index]) {
        spi_model_select(bus->slaves[index], selected);
    }
}

uint8_t spi_bus_transmit(struct spi_bus *bus, uint8_t value) {
    unsigned int index = bus->selected[0] ? 0 : 1;

    if (bus->selected[0] == bus->selected[1] || !bus->slaves[index]) {
        bus->selection_errors++;
        return 0xFF;
    }
    bus->bytes_transferred++;
    return spi_model_transmit(bus->slaves[index], value);
}
