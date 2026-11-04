#include "tmp117.h"
#include "i2c.h"

#define TMP117_REG_TEMPERATURE 0x00u
#define TMP117_REG_CONFIGURATION 0x01u
#define TMP117_REG_DEVICE_ID 0x0fu
#define TMP117_DATA_READY 0x2000u

static int read_word(uint8_t addr, uint8_t reg, uint16_t *word) {

    uint8_t bytes[2];

    int status = i2c2_read_reg(addr, reg, bytes, sizeof(bytes));

    if (status != I2C_OK)
        return status;

    *word = ((uint16_t)bytes[0] << 8) | bytes[1];

    return TMP117_OK;
}

int tmp117_check(uint8_t addr) {

    uint16_t device_id;

    int status = read_word(addr, TMP117_REG_DEVICE_ID, &device_id);

    if (status != TMP117_OK)
        return status;

    // Bits 15:12 are the revision and may differ between devices
    return (device_id & 0x0fffu) == 0x0117u ? TMP117_OK : TMP117_ERR_ID;
}

int tmp117_read(uint8_t addr, int32_t *temperature) {

    if (temperature == NULL)
        return I2C_ERR_ARGUMENT;

    uint16_t word;

    int status = read_word(addr, TMP117_REG_CONFIGURATION, &word);

    if (status != TMP117_OK)
        return status;

    if (!(word & TMP117_DATA_READY))
        return TMP117_NOT_READY;

    status = read_word(addr, TMP117_REG_TEMPERATURE, &word);

    if (status != TMP117_OK)
        return status;

    if (word == 0x8000u)
        return TMP117_NOT_READY;

    /* TMP117 sends a big-endian two's-complement word at 1/128 C per LSB.
     * Explicit subtraction avoids implementation-defined unsigned-to-signed
     * narrowing. The scaled intermediate comfortably fits in int32_t.
     */
    int32_t raw = (word & 0x8000u) ? (int32_t)word - 65536 : (int32_t)word;

    int32_t scaled = raw * 1000;

    *temperature = (scaled >= 0 ? scaled + 64 : scaled - 64) / 128;

    return TMP117_OK;
}
