#ifndef PAYLOAD_SPI_H
#define PAYLOAD_SPI_H

#include <stdint.h>
#include <config.h>
#include <image_protocol.h>

#if PAYLOAD_SPI_MASTER_MBR < 0 || PAYLOAD_SPI_MASTER_MBR > 7
#error "SPI master prescaler encoding must be between 0 and 7"
#endif
#if PAYLOAD_SPI_POLL_LIMIT < 1 || PAYLOAD_SPI_DMA_STOP_LIMIT < 1
#error "SPI poll limits must be nonzero"
#endif
#if PAYLOAD_SPI_SCK_SPEED < 0 || PAYLOAD_SPI_SCK_SPEED > 3 || \
    AVIONICS_SPI_SCK_SPEED < 0 || AVIONICS_SPI_SCK_SPEED > 3
#error "SPI GPIO speed encodings must be between 0 and 3"
#endif
#if COMMS_IMAGE_REQUEST_TIMEOUT_MS < 1 || COMMS_TELEMETRY_REQUEST_TIMEOUT_MS < 1 || \
    PAYLOAD_SPI_SLAVE_TIMEOUT_MS < 1 || COMMS_IMAGE_CRC_BLOCK_SIZE < 1 || \
    COMMS_IMAGE_REQUEST_TIMEOUT_MS > 0x7fffffffU || COMMS_TELEMETRY_REQUEST_TIMEOUT_MS > 0x7fffffffU || \
    PAYLOAD_SPI_SLAVE_TIMEOUT_MS > 0x7fffffffU
#error "SPI timeouts and image checksum block size must be nonzero"
#endif
#if COMMS_TELEMETRY_REQUEST_TIMEOUT_MS >= TELEMETRY_SAMPLE_INTERVAL_MS
#error "Telemetry request timeout must be shorter than its sampling/poll interval"
#endif
#if PAYLOAD_SPI_CS_SETTLE_CYCLES < 1
#error "SPI chip select settle cycles must be positive"
#endif
#if COMMS_PAYLOAD_CS_GPIO_PORT < 0 || COMMS_PAYLOAD_READY_GPIO_PORT < 0 || \
    COMMS_AVIONICS_CS_GPIO_PORT < 0 || COMMS_AVIONICS_READY_GPIO_PORT < 0 || \
    AVIONICS_SPI_CS_GPIO_PORT < 0 || AVIONICS_SPI_READY_GPIO_PORT < 0 || \
    PAYLOAD_SPI_READY_GPIO_PORT < 0 || \
    COMMS_PAYLOAD_CS_GPIO_PORT > 10 || COMMS_PAYLOAD_READY_GPIO_PORT > 10 || \
    COMMS_AVIONICS_CS_GPIO_PORT > 10 || COMMS_AVIONICS_READY_GPIO_PORT > 10 || \
    AVIONICS_SPI_CS_GPIO_PORT > 10 || AVIONICS_SPI_READY_GPIO_PORT > 10 || \
    PAYLOAD_SPI_READY_GPIO_PORT > 16 || \
    (PAYLOAD_SPI_READY_GPIO_PORT > 7 && PAYLOAD_SPI_READY_GPIO_PORT < 13)
#error "SPI GPIO port number is outside the supported range"
#endif
#if COMMS_PAYLOAD_CS_GPIO_PIN < 0 || COMMS_PAYLOAD_READY_GPIO_PIN < 0 || \
    COMMS_AVIONICS_CS_GPIO_PIN < 0 || COMMS_AVIONICS_READY_GPIO_PIN < 0 || \
    AVIONICS_SPI_CS_GPIO_PIN < 0 || AVIONICS_SPI_READY_GPIO_PIN < 0 || \
    PAYLOAD_SPI_READY_GPIO_PIN < 0 || \
    COMMS_PAYLOAD_CS_GPIO_PIN > 15 || COMMS_PAYLOAD_READY_GPIO_PIN > 15 || \
    COMMS_AVIONICS_CS_GPIO_PIN > 15 || COMMS_AVIONICS_READY_GPIO_PIN > 15 || \
    AVIONICS_SPI_CS_GPIO_PIN > 15 || AVIONICS_SPI_READY_GPIO_PIN > 15 || \
    PAYLOAD_SPI_READY_GPIO_PIN > 15
#error "SPI GPIO pin must be between 0 and 15"
#endif

/* Fixed-size mode-0 transfers on D11..D13 with separate slave selects. */
#define PAYLOAD_SPI_SIZE IMAGE_SPI_FRAME_SIZE

enum payload_spi_rx_status {
    PAYLOAD_SPI_RX_RECEIVING = 1U << 0,
    PAYLOAD_SPI_RX_READY = 1U << 1,
    PAYLOAD_SPI_RX_PROCESSING = 1U << 2,
    PAYLOAD_SPI_RX_ERROR = 1U << 3,
};

typedef struct {
    uint8_t data[PAYLOAD_SPI_SIZE] __attribute__((aligned(32)));
    volatile uint32_t status;
    volatile uint32_t sequence;
} payload_spi_rx_buffer_t;

/* DMA never overwrites a READY or PROCESSING receive buffer. */
extern payload_spi_rx_buffer_t payload_spi_rx_buffer;
/* Claim a complete frame; release it after the application handles it. */
int payload_spi_receive_acquire(const uint8_t **frame);
void payload_spi_receive_release(void);
/* True while a slave is completing an already selected SPI frame. */
int payload_spi_busy(void);

/* H753 SPI1 prescaler encoding: 5 = 1 MHz, 4 = 2 MHz, 3 = 4 MHz
 * with the existing 64 MHz HSI source. Does not change any RCC clocks. */
#define PAYLOAD_SPI_MASTER_CLOCK_HZ (64000000U / (2U << PAYLOAD_SPI_MASTER_MBR))

enum payload_spi_target {
    PAYLOAD_SPI_TARGET_PAYLOAD,
    PAYLOAD_SPI_TARGET_AVIONICS,
};

/* Radio: SPI1 master. Avionics: SPI1 slave. Payload: SPI5 slave. */
int payload_spi_init(void);
/* Radio only: read the READY wire immediately. Payload advertises availability;
 * avionics asserts READY only after its software chip select is driven low. */
int payload_spi_target_ready(enum payload_spi_target target);
/* Avionics selection waits for arming and for MISO release after CS rises.
 * Failed selections are cleaned up before either slave can be selected again. */
int payload_spi_exchange_target(enum payload_spi_target target,
                                const uint8_t tx[PAYLOAD_SPI_SIZE],
                                uint8_t rx[PAYLOAD_SPI_SIZE]);
/* Radio only: clocks one payload frame; returns 0 or -1 on failure.
 * A null rx leaves the completed frame READY for receive_acquire(). */
int payload_spi_exchange(const uint8_t tx[PAYLOAD_SPI_SIZE],
                         uint8_t rx[PAYLOAD_SPI_SIZE]);
/* Slaves: returns 1 for a complete frame after CS rises, 0 if idle,
 * or -1 for a short frame/peripheral error. Never waits for the master. */
int payload_spi_service(uint8_t request[PAYLOAD_SPI_SIZE]);
/* Slaves: replace the reply for the next exchange while paused. */
void payload_spi_set_response(const uint8_t response[PAYLOAD_SPI_SIZE]);
/* Slaves: discard partial work before camera capture or sensor access. */
void payload_spi_pause(void);

#endif
