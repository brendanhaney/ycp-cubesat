#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "spi_renode.h"

static pthread_mutex_t model_lock = PTHREAD_MUTEX_INITIALIZER;

void spi_renode_check(renode_error_t *error) {
    if (error) {
        fprintf(stderr, "Renode API error: %s\n", error->message ? error->message : "unknown");
        renode_free_error(error);
        exit(EXIT_FAILURE);
    }
}

void spi_renode_assert(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "SPI assertion failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void on_register(void *user_data, renode_sysbus_event_data_t *event) {
    struct spi_endpoint *endpoint = user_data;
    uint32_t width = (uint32_t)event->width;
    uint32_t value = 0;

    // Firmware uses aligned byte TXDR/RXDR and 32-bit control/status accesses.
    if (event->transfer_count != 1 || (width != 1 && width != 2 && width != 4) ||
        event->address < SPI_MODEL_BASE ||
        event->address + width > SPI_MODEL_BASE + SPI_MODEL_SIZE ||
        (event->address % 4)) {
        return;
    }
    unsigned int offset = (unsigned int)(event->address - SPI_MODEL_BASE);
    pthread_mutex_lock(&model_lock);
    if (event->access_type == SYSBUS_CB_WRITE) {
        for (uint32_t i = 0; i < width; i++) {
            value |= (uint32_t)event->data[i] << (i * 8);
        }
        spi_model_write(&endpoint->model, offset, value);
    } else {
        value = spi_model_read(&endpoint->model, offset);
        for (uint32_t i = 0; i < width; i++) {
            event->data[i] = (uint8_t)(value >> (i * 8));
        }
    }
    pthread_mutex_unlock(&model_lock);
    event->access_succeeded = true;
}

static uint8_t on_transmit(void *user_data, uint8_t mosi) {
    struct spi_simulation *simulation = user_data;
    pthread_mutex_lock(&model_lock);
    uint8_t miso = spi_bus_transmit(&simulation->bus, mosi);
    pthread_mutex_unlock(&model_lock);
    return miso;
}

static void on_select(void *user_data, renode_gpio_event_data_t *event) {
    struct spi_endpoint *endpoint = user_data;
    pthread_mutex_lock(&model_lock);
    spi_bus_select(&endpoint->simulation->bus, endpoint->index, !event->state);
    pthread_mutex_unlock(&model_lock);
    if (endpoint->gpio_e) {
        spi_renode_check(renode_set_gpio_state(endpoint->gpio_e, 4, event->state));
    }
}

static void on_ready(void *user_data, renode_gpio_event_data_t *event) {
    struct spi_endpoint *endpoint = user_data;
    if (endpoint->simulation->gpio_b) {
        spi_renode_check(renode_set_gpio_state(endpoint->simulation->gpio_b,
            (int32_t)endpoint->index, event->state));
    }
}

static void connect_simulation(struct spi_simulation *simulation, const char *module) {
    const char *names[] = {"radio", "payload"};
    int all = !strcmp(module, "all");

    if (all || !strcmp(module, "avionics")) {
        spi_renode_check(renode_get_machine(simulation->renode, "avionics", &simulation->master));
        spi_renode_check(renode_get_gpio(simulation->master, "sysbus.gpioPortE", &simulation->gpio_e));
        spi_renode_check(renode_get_gpio(simulation->master, "sysbus.gpioPortB", &simulation->gpio_b));
        spi_renode_check(renode_get_sysbus(simulation->master, &simulation->sysbus));
        spi_renode_check(renode_get_spi(simulation->master, "sysbus.spi4.spiBus", &simulation->spi));
        spi_renode_check(renode_spi_register_callbacks(simulation->spi, simulation, on_transmit, NULL));
    }
    for (unsigned int i = 0; i < 2; i++) {
        struct spi_endpoint *endpoint = &simulation->endpoints[i];
        endpoint->simulation = simulation;
        endpoint->index = i;
        if (all || !strcmp(module, names[i])) {
            spi_renode_check(renode_get_machine(simulation->renode, names[i], &endpoint->machine));
            spi_renode_check(renode_get_bus_peripheral(endpoint->machine, "sysbus.spi4", &endpoint->peripheral));
            spi_renode_check(renode_register_sysbus_access_callback(endpoint->peripheral,
                SYSBUS_CB_BOTH, AW_CB_ANY, endpoint, on_register));
            spi_renode_check(renode_get_gpio(endpoint->machine, "sysbus.gpioPortE", &endpoint->gpio_e));
            spi_renode_check(renode_get_gpio(endpoint->machine, "sysbus.gpioPortB", &endpoint->gpio_b));
            spi_renode_check(renode_set_gpio_state(endpoint->gpio_e, 4, true));
            spi_renode_check(renode_register_gpio_state_change_callback(endpoint->gpio_b, 0, endpoint, on_ready));
            simulation->bus.slaves[i] = &endpoint->model;
        }
        if (simulation->master) {
            spi_renode_check(renode_set_gpio_state(simulation->gpio_b, (int32_t)i, false));
            spi_renode_check(renode_register_gpio_state_change_callback(simulation->gpio_e,
                (int32_t)i + 3, endpoint, on_select));
        }
    }
}

int main(int argc, char **argv) {
    struct spi_simulation simulation = {0};
    renode_time_t duration;

    if ((argc != 3 && argc != 4) ||
        (argc == 4 && strcmp(argv[3], "--stay-connected")) ||
        (strcmp(argv[2], "all") && strcmp(argv[2], "avionics") &&
        strcmp(argv[2], "radio") && strcmp(argv[2], "payload"))) {
        fprintf(stderr, "Usage: %s PORT all|avionics|radio|payload [--stay-connected]\n", argv[0]);
        return EXIT_FAILURE;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    spi_renode_check(renode_connect(argv[1], &simulation.renode));
    connect_simulation(&simulation, argv[2]);
    puts("C SPI peripheral models connected");
    spi_renode_check(renode_create_time(!strcmp(argv[2], "all") ? 10 : 1,
        TU_MILLISECONDS, &duration));
    spi_renode_check(renode_run_for(simulation.renode, duration));
    printf("Firmware SPI bus: %u bytes, %u selection errors\n",
        simulation.bus.bytes_transferred, simulation.bus.selection_errors);
#ifdef SPI_INTEGRATION_TEST
    spi_assert_integration(&simulation);
    spi_assert_wiring(&simulation);
#endif
    if (argc == 4) {
        // The API's callback thread continues serving peripherals while the
        // user inspects or resumes the paused simulation in Renode's monitor.
        puts("SPI adapter remains connected for the Renode GUI");
        (void)getchar();
    }
    spi_renode_check(renode_disconnect(&simulation.renode));
    free(simulation.master);
    free(simulation.gpio_e);
    free(simulation.gpio_b);
    free(simulation.spi);
    free(simulation.sysbus);
    for (unsigned int i = 0; i < 2; i++) {
        free(simulation.endpoints[i].machine);
        free(simulation.endpoints[i].peripheral);
        free(simulation.endpoints[i].gpio_e);
        free(simulation.endpoints[i].gpio_b);
    }
    return EXIT_SUCCESS;
}
