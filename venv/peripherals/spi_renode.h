#ifndef SPI_RENODE_H
#define SPI_RENODE_H

#include "renode_api.h"
#include "spi_model.h"

struct spi_simulation;
struct spi_endpoint {
    struct spi_simulation *simulation;
    unsigned int index;
    struct spi_slave model;
    renode_machine_t *machine;
    renode_bus_peripheral_t *peripheral;
    renode_gpio_t *gpio_e, *gpio_b;
};

struct spi_simulation {
    renode_t *renode;
    renode_machine_t *master;
    renode_gpio_t *gpio_e, *gpio_b;
    renode_spi_t *spi;
    renode_bus_context_t *sysbus;
    struct spi_bus bus;
    struct spi_endpoint endpoints[2];
};

void spi_renode_check(renode_error_t *error);
void spi_renode_assert(int condition, const char *message);
// Implemented only by the integration-test executable.
void spi_assert_integration(struct spi_simulation *simulation);
void spi_assert_wiring(struct spi_simulation *simulation);

#endif
