#include "drivers/uart.h"
#include "utils/delay.h"
#include "utils/uptime.h"
#include <image_protocol.h>
#include <payload_spi.h>
#include <telemetry_protocol.h>
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

static uint32_t sequence;
static uint8_t response[IMAGE_SPI_FRAME_SIZE];
static uint8_t telemetry_response[IMAGE_SPI_FRAME_SIZE];
static uint32_t last_telemetry_poll;
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
        if (target == PAYLOAD_SPI_TARGET_PAYLOAD &&
            !payload_spi_target_ready(target))
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
                         response, 250U);
}

static int32_t telemetry_value(enum telemetry_word word) {
    return (int32_t)image_get_u32(telemetry_response + IMAGE_SPI_DATA_OFFSET + word * 4U);
}

static void service_telemetry(void) {

    const uint32_t now = uptime_ms();

    if ((uint32_t)(now - last_telemetry_poll) < TELEMETRY_INTERVAL_MS)
        return;

    last_telemetry_poll = now;

    if (request_frame(PAYLOAD_SPI_TARGET_AVIONICS, TELEMETRY_OP_READ, 0, 0, telemetry_response, 500U) != IMAGE_STATUS_OK) {

        debug_telemetry_errors++;

        uart_printf("Avionics: waiting for telemetry\n");

        return;
    }

    if (image_get_u16(telemetry_response + 6) != TELEMETRY_DATA_SIZE ||
        image_get_u32(telemetry_response + 20) != TELEMETRY_DATA_SIZE ||
        image_get_u32(telemetry_response + 12) != (uint32_t)telemetry_value(TELEMETRY_COUNTER)) {

        debug_telemetry_errors++;

        uart_printf("Avionics: invalid telemetry response\n");

        return;
    }

    debug_telemetry_received++;

    debug_telemetry_counter = (uint32_t)telemetry_value(TELEMETRY_COUNTER);

    uart_printf("Telemetry %d: sample %d ms, received %d ms, CRC %x OK\n",
                debug_telemetry_counter, telemetry_value(TELEMETRY_TIME_MS),
                uptime_ms(), image_get_u32(telemetry_response + IMAGE_SPI_CRC_OFFSET));

    for (unsigned int i = 0; i < 2; ++i) {

        const enum telemetry_word status = i ? TELEMETRY_TEMP2_STATUS : TELEMETRY_TEMP1_STATUS;

        if (telemetry_value(status) == 0)
            uart_printf("Temperature %d: %f C\n", i + 1U,
                        (double)telemetry_value(status + 1) / 1000.0);

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

        uart_printf("IMU: status %d\n", telemetry_value(TELEMETRY_IMU_STATUS));

    }
}

static int send_image(uint32_t id, uint32_t length, uint32_t crc) {

    uint32_t offset = 0;
    uint32_t received_crc = 0;

    debug_image_expected_crc = crc;

    while (offset < length) {

        if (request(IMAGE_OP_READ, id, offset) != IMAGE_STATUS_OK)
            return -1;

        const uint32_t remaining = length - offset;
        const uint32_t count = remaining < IMAGE_SPI_DATA_SIZE ? remaining : IMAGE_SPI_DATA_SIZE;

        if (image_get_u16(response + 6) != count ||
            image_get_u32(response + 12) != id ||
            image_get_u32(response + 16) != offset ||
            image_get_u32(response + 20) != length ||
            image_get_u32(response + 24) != crc)
            return -1;

        received_crc = image_crc32_update(received_crc, response + IMAGE_SPI_DATA_OFFSET, count);

        offset += count;
    }

    if (received_crc != crc) {
        debug_image_crc_errors++;
        uart_printf("Image %d CRC mismatch: expected %x received %x\n", id, crc, received_crc);
        return -1;
    }

    debug_image_id = id;
    debug_image_bytes = length;
    debug_image_crc = crc;
    debug_images_sent++;

    uart_printf("Image %d: %d bytes, CRC %x == %x OK\n", id, length, crc, received_crc);

    return 0;
}

int main(void) {

    uart_init();

    if (uart_set_baud(IMAGE_UART_BAUD)) {
        uart_printf("Radio: UART baud setup failed\n");
        return 1;
    }

    uart_printf("Radio: image CRC verification, UART %d baud\n", IMAGE_UART_BAUD);

    if (delay_init(64000000U) || uptime_init(64000000U) || payload_spi_init()) {
        uart_printf("Radio: timer/SPI DMA initialization failed\n");
        return 1;
    }

    uart_printf("Radio: shared SPI, avionics telemetry every 10 seconds\n");
    last_telemetry_poll = uptime_ms() - TELEMETRY_INTERVAL_MS;

    while (1) {
        service_telemetry();

        /* A zero image ID requests a lease on the latest saved image,
         * including one that this board has already validated. */
        if (request(IMAGE_OP_INFO, 0, 0) == IMAGE_STATUS_OK) {

            const uint32_t id = image_get_u32(response + 12);
            const uint32_t length = image_get_u32(response + 20);
            const uint32_t crc = image_get_u32(response + 24);

            if (id && length && length <= IMAGE_MAX_JPEG_SIZE) {

                if (send_image(id, length, crc))
                    debug_spi_errors++;

                request(IMAGE_OP_RELEASE, id, 0);
            }
        }
    }
}
