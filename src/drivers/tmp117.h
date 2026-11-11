// SOURCES
// https://www.ti.com/lit/ds/symlink/tmp117.pdf
// https://cdn-learn.adafruit.com/downloads/pdf/adafruit-tmp117-high-accuracy-i2c-temperature-monitor.pdf
// TI datasheet Table 7-2: ADD0 = GND/V+/SDA/SCL selects 0x48/0x49/0x4A/0x4B.

#ifndef TMP117_DRIVER_H
#define TMP117_DRIVER_H

#include <stdint.h>

enum {
    TMP117_OK = 0,
    TMP117_NOT_READY = 1,
    TMP117_ERR_ID = -6
};

typedef struct {

    // 7-bit i2c address
    uint8_t addr;
    uint8_t ready;
    int32_t temp;

} tmp177_t;

// Each call uses an unshifted 7-bit address; devices share no driver state.
int tmp117_check(uint8_t addr);

// will return temperature as millicelsius
int tmp117_read(uint8_t addr, int32_t *temp);

#endif
