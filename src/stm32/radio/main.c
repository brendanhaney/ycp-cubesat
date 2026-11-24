#include "drivers/uart.h"
#include "utils/delay.h"
#include "utils/uptime.h"
#include <image_protocol.h>
#include <payload_spi.h>
#include <telemetry_protocol.h>
#include <comms_buffers.h>
#include <string.h>

volatile unsigned int debug_spi_requests;
volatile unsigned int debug_spi_errors;
volatile unsigned int debug_images_sent;
volatile unsigned int debug_image_id;
volatile unsigned int debug_image_bytes;
volatile unsigned int debug_image_crc;
volatile unsigned int debug_image_expected_crc;
volatile unsigned int debug_image_crc_errors;
volatile unsigned int debug_telemetry_received;
volatile unsigned int debug_telemetry_errors;
volatile unsigned int debug_telemetry_counter;
volatile unsigned int debug_spi_timeouts;
volatile unsigned int debug_images_received;
volatile unsigned int debug_telemetry_skipped;

static uint32_t sequence;
static uint8_t response[IMAGE_SPI_FRAME_SIZE];
static uint8_t telemetry_response[IMAGE_SPI_FRAME_SIZE];
static uint32_t last_telemetry_poll;
static uint32_t last_telemetry_print;
static uint32_t latest_telemetry[TELEMETRY_WORDS];
static uint32_t latest_telemetry_crc;
static uint32_t latest_telemetry_received_ms;
static int latest_telemetry_valid;
static void service_telemetry(void);

/* Replies are prepared after a command arrives. Repeating a command with the
 * same sequence recovers requests lost while the payload captures or saves. */
static int request_frame(enum payload_spi_target target, uint8_t opcode,
                         uint32_t image_id, uint32_t offset, uint8_t *reply,
                         uint32_t timeout_ms) {

    const uint32_t current = ++sequence;
    uint8_t command[IMAGE_SPI_FRAME_SIZE];

    image_frame_init(command, opcode, 0, current, image_id, offset, 0, 0);

    image_frame_seal(command);

    uint32_t started = uptime_ms();

    while ((uint32_t)(uptime_ms() - started) < timeout_ms) {

        if (target == PAYLOAD_SPI_TARGET_PAYLOAD) {
            const uint32_t before = uptime_ms();
            service_telemetry();
            started += uptime_ms() - before;
        }

        /* Avionics raises READY only after the master selects it. The
         * payload advertises a prepared frame before CS is asserted. */
        if (target == PAYLOAD_SPI_TARGET_PAYLOAD && !payload_spi_target_ready(target))
            continue;

        if (!payload_spi_exchange_target(target, command, NULL)) {

            const uint8_t *received;

            if (payload_spi_receive_acquire(&received)) {

                const int valid = image_frame_valid(received) &&
                    received[4] == (opcode | IMAGE_RESPONSE_FLAG) &&
                    image_get_u32(received + 8) == current;

                if (valid)
                    memcpy(reply, received, IMAGE_SPI_FRAME_SIZE);

                payload_spi_receive_release();

                if (valid) {

                    debug_spi_requests++;

                    return reply[5];
                }
            }
        }
    }

    debug_spi_timeouts++;

    return -1;
}

static int request(uint8_t opcode, uint32_t image_id, uint32_t offset) {
    return request_frame(PAYLOAD_SPI_TARGET_PAYLOAD, opcode, image_id, offset,
                         response, COMMS_IMAGE_REQUEST_TIMEOUT_MS);
}

static int32_t telemetry_value(enum telemetry_word word) {
    return (int32_t)latest_telemetry[word];
}

static void print_telemetry(void) {

    uart_printf("\nTelemetry %d: \n", debug_telemetry_counter);

    for (unsigned int i = 0; i < 2; ++i) {

        const enum telemetry_word status = i ? TELEMETRY_TEMP2_STATUS : TELEMETRY_TEMP1_STATUS;

        if (telemetry_value(status) == 0)
            uart_printf("Temperature %d: %f C\n", i + 1U, (double)telemetry_value(status + 1) / 1000.0);

        else
            uart_printf("Thermometer %d: status %d\n", i + 1U, telemetry_value(status));
    }

    if (telemetry_value(TELEMETRY_BARO_STATUS) == 0) {

        uart_printf("Pressure: %f hPa\n", (double)telemetry_value(TELEMETRY_PRESSURE_PA) / 100.0);

        uart_printf("Barometer temperature: %f C\n", (double)telemetry_value(TELEMETRY_BARO_CC) / 100.0);

    } else {

        uart_printf("Barometer: status %d\n", telemetry_value(TELEMETRY_BARO_STATUS));

    }

    if (telemetry_value(TELEMETRY_IMU_STATUS) == 0) {

        uart_printf("Accelerometer: X=%f Y=%f Z=%f g\n",
                    (double)telemetry_value(TELEMETRY_ACCEL_X_MG) / 1000.0,
                    (double)telemetry_value(TELEMETRY_ACCEL_Y_MG) / 1000.0,
                    (double)telemetry_value(TELEMETRY_ACCEL_Z_MG) / 1000.0);

        uart_printf("Gyroscope: X=%f Y=%f Z=%f deg/s\n",
                    (double)telemetry_value(TELEMETRY_GYRO_X_MDPS) / 1000.0,
                    (double)telemetry_value(TELEMETRY_GYRO_Y_MDPS) / 1000.0,
                    (double)telemetry_value(TELEMETRY_GYRO_Z_MDPS) / 1000.0);

    } else {

        uart_printf("IMU: status %d\n\n", telemetry_value(TELEMETRY_IMU_STATUS));

    }
}

static void service_telemetry(void) {

    const uint32_t now = uptime_ms();

    if ((uint32_t)(now - last_telemetry_poll) >= TELEMETRY_SAMPLE_INTERVAL_MS) {

        last_telemetry_poll = now;

        if (comms_telemetry_begin() == 0) {

            if (request_frame(PAYLOAD_SPI_TARGET_AVIONICS, TELEMETRY_OP_READ,
                              0, 0, telemetry_response,
                              COMMS_TELEMETRY_REQUEST_TIMEOUT_MS) == IMAGE_STATUS_OK &&
                              image_get_u16(telemetry_response + 6) == TELEMETRY_DATA_SIZE &&
                              image_get_u32(telemetry_response + 20) == TELEMETRY_DATA_SIZE &&
                              image_get_u32(telemetry_response + 12) ==
                              image_get_u32(telemetry_response + IMAGE_SPI_DATA_OFFSET) &&
                              comms_telemetry_complete(telemetry_response + IMAGE_SPI_DATA_OFFSET,
                              TELEMETRY_DATA_SIZE,
                              image_get_u32(telemetry_response + IMAGE_SPI_CRC_OFFSET)) == 0) {

                debug_telemetry_received++;

            } else {

                debug_telemetry_errors++;

                comms_buffer_abort(&comms_telemetry_buffer.flags);

            }

        } else {

            debug_telemetry_skipped++;

        }
    }

    if (comms_buffer_acquire(&comms_telemetry_buffer.flags)) {

        memcpy(latest_telemetry, comms_telemetry_buffer.words, sizeof(latest_telemetry));

        latest_telemetry_crc = comms_telemetry_buffer.crc32;

        latest_telemetry_received_ms = uptime_ms();

        latest_telemetry_valid = 1;

        debug_telemetry_counter = latest_telemetry[TELEMETRY_COUNTER];

        comms_buffer_release(&comms_telemetry_buffer.flags);

    }

    if ((uint32_t)(uptime_ms() - last_telemetry_print) >= TELEMETRY_INTERVAL_MS) {

        last_telemetry_print = uptime_ms();

        if (latest_telemetry_valid)
            print_telemetry();

        else
            uart_printf("Avionics: waiting for telemetry\n");

    }
}

static int receive_image(uint32_t id, uint32_t length, uint32_t crc) {

    if (comms_image_begin(id, length, crc))
        return -1;

    uint32_t offset = 0;
    int result = -1;

    while (offset < length) {

        if (request(IMAGE_OP_READ, id, offset) != IMAGE_STATUS_OK)
            goto release;

        const uint32_t remaining = length - offset;
        const uint32_t count = remaining < IMAGE_SPI_DATA_SIZE ? remaining : IMAGE_SPI_DATA_SIZE;

        if (image_get_u16(response + 6) != count ||
            image_get_u32(response + 12) != id ||
            image_get_u32(response + 16) != offset ||
            image_get_u32(response + 20) != length ||
            image_get_u32(response + 24) != crc ||
            comms_image_append(offset, response + IMAGE_SPI_DATA_OFFSET, count))
            goto release;

        offset += count;
    }
    result = 0;

release:
    /* The N6 can reuse its JPEG RAM while comms handles its complete copy. */
    if (request(IMAGE_OP_RELEASE, id, 0) != IMAGE_STATUS_OK)
        debug_spi_errors++;

    if (!result && !comms_image_complete()) {
        debug_images_received++;

        return 0;
    }

    comms_buffer_abort(&comms_image_buffer.flags);

    return -1;
}

static void handle_image(void) {
    if (!comms_buffer_acquire(&comms_image_buffer.flags))
        return;

    const uint32_t id = comms_image_buffer.image_id;
    const uint32_t length = comms_image_buffer.length;
    debug_image_id = id;
    debug_image_bytes = length;

#if IMAGE_CRC_ENABLED
    const uint32_t crc = comms_image_buffer.crc32;

    uint32_t received_crc = 0;
    uint32_t offset = 0;

    while (offset < length) {

        service_telemetry();

        uint32_t count;

        const uint8_t *data = comms_image_data(offset, &count);

        if (!data || !count) {

            debug_spi_errors++;

            comms_buffer_abort(&comms_image_buffer.flags);

            return;
        }

        if (count > COMMS_IMAGE_CRC_BLOCK_SIZE)
            count = COMMS_IMAGE_CRC_BLOCK_SIZE;

        received_crc = image_crc32_update(received_crc, data, count);

        offset += count;
    }

    debug_image_expected_crc = crc;
    debug_image_crc = received_crc;

    if (received_crc != crc) {

        debug_image_crc_errors++;

        uart_printf("Image %d CRC mismatch: expected %x received %x\n", id, crc, received_crc);

        comms_buffer_abort(&comms_image_buffer.flags);

        return;
    }
#else
    debug_image_expected_crc = 0;
    debug_image_crc = 0;
#endif

    debug_images_sent++;

#if IMAGE_CRC_ENABLED
    uart_printf("Image %d: %d bytes, CRC %x == %x\n", id, length, crc, received_crc);
#else
    uart_printf("Image %d: read %d bytes\n", id, length);
#endif

    comms_buffer_release(&comms_image_buffer.flags);
}

int main(void) {

    uart_init();

    if (uart_set_baud(IMAGE_UART_BAUD)) {

        uart_printf("Radio: UART baud setup failed\n");

        return 1;
    }

#if IMAGE_CRC_ENABLED
    uart_printf("Radio: image CRC verification, UART %d baud\n", IMAGE_UART_BAUD);
#else
    uart_printf("Radio: image CRC disabled, UART %d baud\n", IMAGE_UART_BAUD);
#endif

    if (delay_init(64000000U) || uptime_init(64000000U) || payload_spi_init()) {

        uart_printf("Radio: timer/SPI DMA initialization failed\n");

        return 1;
    }

    comms_buffers_init();

    uart_printf("Radio: shared SPI, telemetry receive %d ms, print %d ms\n", TELEMETRY_SAMPLE_INTERVAL_MS, TELEMETRY_INTERVAL_MS);

    last_telemetry_poll = uptime_ms() - TELEMETRY_SAMPLE_INTERVAL_MS;

    last_telemetry_print = uptime_ms() - TELEMETRY_INTERVAL_MS;

    while (1) {

        service_telemetry();

        handle_image();

        if (!(comms_image_buffer.flags & COMMS_BUFFER_READY))
            continue;

        /* A zero image ID requests a lease on the latest saved image,
         * including one that this board has already validated. */
        if (request(IMAGE_OP_INFO, 0, 0) == IMAGE_STATUS_OK) {

            const uint32_t id = image_get_u32(response + 12);
            const uint32_t length = image_get_u32(response + 20);
            const uint32_t crc = image_get_u32(response + 24);

            if (id && length && length <= IMAGE_MAX_JPEG_SIZE) {

                if (receive_image(id, length, crc))
                    debug_spi_errors++;
            } else if (id) {
                request(IMAGE_OP_RELEASE, id, 0);
            }
        }
    }
}
