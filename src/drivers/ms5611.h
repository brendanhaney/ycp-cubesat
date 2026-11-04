// SOURCES:
// https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS5611-01BA03%7FB3%7Fpdf%7FEnglish%7FENG_DS_MS5611-01BA03_B3.pdf%7FMS561101BA03-50
// https://github.com/TEConnectivity/MS5611_Generic_C_Driver/blob/master/ms5611.c
// https://www.mikroe.com/brochure?ref=MIKROE-4903
// https://www.mikroe.com/altitude-6-click
// https://download.mikroe.com/documents/add-on-boards/click/altitude_6_click/Altitude_6_click_v100_Schematic.PDF

#ifndef MS5611_DRIVER_H
#define MS5611_DRIVER_H

#include <stdint.h>

enum {
    MS5611_OK = 0,
    MS5611_ERR_CALIBRATION = -7,
    MS5611_ERR_DATA = -8,
    MS5611_ERR_NOT_INITIALIZED = -9
};

typedef struct {
    uint8_t address;
    uint8_t last_command;
    uint16_t calibration[6];
    int initialized;
} ms5611_t;

int ms5611_init(ms5611_t *sensor, uint8_t address);
int ms5611_read(ms5611_t *sensor, int32_t *pressure, int32_t *centicelsius);

#endif
