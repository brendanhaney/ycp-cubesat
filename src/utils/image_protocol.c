#include <image_protocol.h>
#include <string.h>
#include "crc.h"

uint16_t image_get_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

uint32_t image_get_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void image_put_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

void image_put_u32(uint8_t *data, uint32_t value) {
    for (unsigned int i = 0; i < 4; i++)
        data[i] = (uint8_t)(value >> (i * 8U));
}

uint32_t image_crc32_update(uint32_t crc, const void *data, size_t length) {
    return crc32(crc, data, length);
}

uint32_t image_crc32(const void *data, size_t length) {
    return image_crc32_update(0, data, length);
}

void image_frame_init(uint8_t *frame, uint8_t opcode, uint8_t status,
                      uint32_t sequence, uint32_t image_id, uint32_t offset,
                      uint32_t image_size, uint32_t image_crc) {
    memset(frame, 0, IMAGE_SPI_FRAME_SIZE);
    memcpy(frame, "YCI1", 4);
    frame[4] = opcode;
    frame[5] = status;
    image_put_u32(frame + 8, sequence);
    image_put_u32(frame + 12, image_id);
    image_put_u32(frame + 16, offset);
    image_put_u32(frame + 20, image_size);
    image_put_u32(frame + 24, image_crc);
}

void image_frame_seal(uint8_t *frame) {
    image_put_u32(frame + IMAGE_SPI_CRC_OFFSET,
                   image_crc32(frame, IMAGE_SPI_CRC_OFFSET));
}

int image_frame_valid(const uint8_t *frame) {
    return memcmp(frame, "YCI1", 4) == 0 &&
        image_get_u16(frame + 6) <= IMAGE_SPI_DATA_SIZE &&
        image_get_u32(frame + IMAGE_SPI_CRC_OFFSET) ==
            image_crc32(frame, IMAGE_SPI_CRC_OFFSET);
}

void image_uart_header(uint8_t *header, uint32_t image_id, uint32_t image_size,
                       uint32_t image_crc) {
    memcpy(header, "YCPJPG1\0", 8);
    image_put_u32(header + 8, image_id);
    image_put_u32(header + 12, image_size);
    image_put_u32(header + 16, image_crc);
    image_put_u32(header + 20, image_crc32(header, 20));
}
