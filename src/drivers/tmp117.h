// SOURCES
// https://www.ti.com/lit/ds/symlink/tmp117.pdf
// https://cdn-learn.adafruit.com/downloads/pdf/adafruit-tmp117-high-accuracy-i2c-temperature-monitor.pdf

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

int tmp117_check(uint8_t addr);

// will return temperature as millicelsius
int tmp117_read(uint8_t addr, int32_t *temp);

#endif
