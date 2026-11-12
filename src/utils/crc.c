#include "crc.h"

#include <typedefs.h>

#define CRC_POLY 0xedb88320

uint32_t crc32(uint32_t crc, const void *buff, size_t len) {

    const uint8_t *bytes = buff;

    int k;

    crc = ~crc;

    while (len--) {

        crc ^= *bytes++;

        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ CRC_POLY : crc >> 1;

    }

    return ~crc;
}
