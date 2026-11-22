#include "drivers/uart.h"
#include "utils/delay.h"
#include <image_protocol.h>
#include <payload_spi.h>
#include <string.h>

volatile unsigned int debug_spi_requests;
volatile unsigned int debug_spi_errors;
volatile unsigned int debug_images_sent;
volatile unsigned int debug_image_id;
volatile unsigned int debug_image_bytes;
volatile unsigned int debug_image_crc;
volatile unsigned int debug_image_expected_crc;
volatile unsigned int debug_image_crc_errors;

static uint32_t sequence;
static uint8_t command[IMAGE_SPI_FRAME_SIZE];
static uint8_t response[IMAGE_SPI_FRAME_SIZE];

/* Replies are prepared after a command arrives. Repeating a command with the
 * same sequence recovers requests lost while the payload captures or saves. */
static int request(uint8_t opcode, uint32_t image_id, uint32_t offset) {

    const uint32_t current = ++sequence;

    image_frame_init(command, opcode, 0, current, image_id, offset, 0, 0);

    image_frame_seal(command);

    for (unsigned int attempt = 0; attempt < 3000; attempt++) {

        if (!payload_spi_exchange(command, NULL)) {
            const uint8_t *received;
            if (payload_spi_receive_acquire(&received)) {
                const int valid = image_frame_valid(received) &&
                    received[4] == (opcode | IMAGE_RESPONSE_FLAG) &&
                    image_get_u32(received + 8) == current;
                if (valid)
                    memcpy(response, received, sizeof(response));
                payload_spi_receive_release();
                if (valid) {
                    debug_spi_requests++;
                    return response[5];
                }
            }
        }
    }

    debug_spi_errors++;

    return -1;
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

    if (delay_init(64000000U) || payload_spi_init()) {
        uart_printf("Radio: SPI DMA initialization failed\n");
        return 1;
    }

    uart_printf("Radio: waiting for payload images\n");

    while (1) {

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
