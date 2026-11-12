#include "drivers/uart.h"
#include "utils/delay.h"
#include <image_protocol.h>
#include <payload_spi.h>

volatile unsigned int debug_spi_requests;
volatile unsigned int debug_spi_errors;
volatile unsigned int debug_images_sent;
volatile unsigned int debug_image_id;
volatile unsigned int debug_image_bytes;
volatile unsigned int debug_image_crc;

static uint32_t sequence;
static uint8_t command[IMAGE_SPI_FRAME_SIZE];
static uint8_t response[IMAGE_SPI_FRAME_SIZE];
static uint8_t poll_frame[IMAGE_SPI_FRAME_SIZE];

/* Replies are prepared after a command arrives. Repeating a command with the
 * same sequence recovers requests lost while the payload captures or saves. */
static int request(uint8_t opcode, uint32_t image_id, uint32_t offset) {
    const uint32_t current = ++sequence;
    image_frame_init(command, opcode, 0, current, image_id, offset, 0, 0);
    image_frame_seal(command);
    for (unsigned int attempt = 0; attempt < 3000; attempt++) {
        const uint8_t *tx = attempt % 8U == 0 ? command : poll_frame;
        if (!payload_spi_exchange(tx, response) && image_frame_valid(response) &&
            response[4] == (opcode | IMAGE_RESPONSE_FLAG) &&
            image_get_u32(response + 8) == current) {
            debug_spi_requests++;
            return response[5];
        }
        delay_ms(1);
    }
    debug_spi_errors++;
    return -1;
}

static int send_image(uint32_t id, uint32_t length, uint32_t crc) {
    uint8_t header[IMAGE_UART_HEADER_SIZE];
    uint32_t offset = 0;
    uint32_t received_crc = 0;

    image_uart_header(header, id, length, crc);
    uart_write(header, sizeof(header));
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
        uart_write(response + IMAGE_SPI_DATA_OFFSET, count);
        received_crc = image_crc32_update(received_crc,
                            response + IMAGE_SPI_DATA_OFFSET, count);
        offset += count;
    }
    if (received_crc != crc)
        return -1;
    debug_image_id = id;
    debug_image_bytes = length;
    debug_image_crc = crc;
    debug_images_sent++;
    return 0;
}

int main(void) {
    uart_init();
    if (uart_set_baud(IMAGE_UART_BAUD) || delay_init(64000000U) || payload_spi_init())
        return 1;
    image_frame_init(poll_frame, IMAGE_OP_NOP, 0, 0, 0, 0, 0, 0);
    image_frame_seal(poll_frame);

    uint32_t last_sent = 0;
    while (1) {
        if (request(IMAGE_OP_INFO, last_sent, 0) == IMAGE_STATUS_OK) {
            const uint32_t id = image_get_u32(response + 12);
            const uint32_t length = image_get_u32(response + 20);
            const uint32_t crc = image_get_u32(response + 24);
            if (id && id != last_sent && length && length <= IMAGE_MAX_JPEG_SIZE) {
                if (!send_image(id, length, crc))
                    last_sent = id;
                else
                    debug_spi_errors++;
                request(IMAGE_OP_RELEASE, id, 0);
            }
        }
        delay_ms(250);
    }
}
