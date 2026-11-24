#ifndef IMAGE_PROTOCOL_H
#define IMAGE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* All multi-byte fields are little endian. SPI replies acknowledge the
 * previous command; NOP polls leave the cached reply intact. */
#define IMAGE_SPI_FRAME_SIZE 256U
#define IMAGE_SPI_DATA_OFFSET 28U
#define IMAGE_SPI_DATA_SIZE 224U
#define IMAGE_SPI_CRC_OFFSET 252U
#define IMAGE_MAX_JPEG_SIZE (800U * 480U * 2U)
#define IMAGE_UART_HEADER_SIZE 24U
#define IMAGE_UART_BAUD 921600U
#define IMAGE_RESPONSE_FLAG 0x80U

enum image_opcode {
    IMAGE_OP_NOP = 0,
    IMAGE_OP_INFO = 1,
    IMAGE_OP_READ = 2,
    IMAGE_OP_RELEASE = 3,
};

enum image_status {
    IMAGE_STATUS_OK = 0,
    IMAGE_STATUS_BUSY = 1,
    IMAGE_STATUS_NO_IMAGE = 2,
    IMAGE_STATUS_STALE = 3,
    IMAGE_STATUS_BAD_REQUEST = 4,
    IMAGE_STATUS_IO_ERROR = 5,
};

uint16_t image_get_u16(const uint8_t *data);
uint32_t image_get_u32(const uint8_t *data);
void image_put_u16(uint8_t *data, uint16_t value);
void image_put_u32(uint8_t *data, uint32_t value);
/* Incremental CRC-32/ISO-HDLC, compatible with zlib.crc32. Start with zero. */
uint32_t image_crc32_update(uint32_t crc, const void *data, size_t length);
uint32_t image_crc32(const void *data, size_t length);
void image_frame_init(uint8_t *frame, uint8_t opcode, uint8_t status,
                      uint32_t sequence, uint32_t image_id, uint32_t offset,
                      uint32_t image_size, uint32_t image_crc);
void image_frame_seal(uint8_t *frame);
int image_frame_valid(const uint8_t *frame);
void image_uart_header(uint8_t *header, uint32_t image_id, uint32_t image_size,
                       uint32_t image_crc);

#endif
