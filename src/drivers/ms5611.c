#include "ms5611.h"
#include "../utils/delay.h"
#include "i2c.h"

#define MS5611_RESET 0x1eu
#define MS5611_PROM_BASE 0xa0u
#define MS5611_CONVERT_PRESSURE 0x48u
#define MS5611_CONVERT_TEMPERATURE 0x58u
#define MS5611_ADC_READ 0x00u

static int send_command(ms5611_t *sensor, uint8_t command) {

    sensor->last_command = command;

    return i2c2_write(sensor->address, &command, sizeof(command));
}

static int read_command(ms5611_t *sensor, uint8_t command, void *data, size_t length) {

    int status = send_command(sensor, command);

    if (status != I2C_OK)
        return status;

    return i2c2_read(sensor->address, data, length);
}

static int calibration_valid(const uint16_t prom[8]) {

    uint16_t remainder = 0;
    int any_nonzero = 0;
    int any_nonfull = 0;

    for (unsigned int i = 0; i < 8; ++i) {

        if (prom[i] != 0)
            any_nonzero = 1;

        if (prom[i] != 0xffffu)
            any_nonfull = 1;

        uint16_t word = i == 7 ? prom[i] & 0xff00u : prom[i];

        for (unsigned int byte = 0; byte < 2; ++byte) {

            remainder ^= byte == 0 ? word >> 8 : word & 0xffu;

            for (unsigned int bit = 0; bit < 8; ++bit) {

                if (remainder & 0x8000u)
                    remainder = (uint16_t)((remainder << 1) ^ 0x3000u);

                else
                    remainder = (uint16_t)(remainder << 1);
            }
        }
    }

    return any_nonzero && any_nonfull &&
           ((remainder >> 12) & 0x0fu) == (prom[7] & 0x0fu);
}

int ms5611_init(ms5611_t *sensor, uint8_t address) {

    if (sensor == NULL)
        return I2C_ERR_ARGUMENT;

    sensor->initialized = 0;

    if (address < 0x08u || address > 0x77u)
        return I2C_ERR_ARGUMENT;

    sensor->address = address;

    int status = send_command(sensor, MS5611_RESET);

    if (status != I2C_OK)
        return status;

    delay_ms(3);

    uint16_t prom[8];

    for (unsigned int i = 0; i < 8; ++i) {

        uint8_t bytes[2];

        status = read_command(sensor, (uint8_t)(MS5611_PROM_BASE + 2u * i), bytes, sizeof(bytes));

        if (status != I2C_OK)
            return status;

        prom[i] = ((uint16_t)bytes[0] << 8) | bytes[1];

    }

    if (!calibration_valid(prom))
        return MS5611_ERR_CALIBRATION;

    for (unsigned int i = 0; i < 6; ++i)
        sensor->calibration[i] = prom[i + 1];

    sensor->initialized = 1;

    return MS5611_OK;
}

static int read_conversion(ms5611_t *sensor, uint8_t command, uint32_t *value) {

    int status = send_command(sensor, command);

    if (status != I2C_OK)
        return status;

    delay_ms(10);

    uint8_t bytes[3];

    status = read_command(sensor, MS5611_ADC_READ, bytes, sizeof(bytes));

    if (status != I2C_OK)
        return status;

    uint32_t raw = ((uint32_t)bytes[0] << 16) |
                   ((uint32_t)bytes[1] << 8) | bytes[2];

    if (raw == 0 || raw == 0xffffffu)
        return MS5611_ERR_DATA;

    *value = raw;

    return MS5611_OK;
}

int ms5611_read(ms5611_t *sensor, int32_t *pressure, int32_t *centicelsius) {

    if (sensor == NULL || pressure == NULL || centicelsius == NULL)
        return I2C_ERR_ARGUMENT;

    if (!sensor->initialized)
        return MS5611_ERR_NOT_INITIALIZED;

    uint32_t raw_pressure;

    int status = read_conversion(sensor, MS5611_CONVERT_PRESSURE, &raw_pressure);

    if (status != MS5611_OK)
        return status;

    uint32_t raw_temperature;

    status = read_conversion(sensor, MS5611_CONVERT_TEMPERATURE, &raw_temperature);

    if (status != MS5611_OK)
        return status;

    const uint16_t *calibration = sensor->calibration;

    int64_t difference = (int64_t)raw_temperature -
                         (int64_t)calibration[4] * 256;

    int64_t temperature = 2000 + difference * calibration[5] / 8388608;
    int64_t offset = (int64_t)calibration[1] * 65536 +
                     (int64_t)calibration[3] * difference / 128;

    int64_t sensitivity = (int64_t)calibration[0] * 32768 +
                          (int64_t)calibration[2] * difference / 256;

    if (temperature < 2000) {

        int64_t temperature_correction = difference * difference / 2147483648LL;
        int64_t cold = temperature - 2000;
        int64_t offset_correction = 5 * cold * cold / 2;
        int64_t sensitivity_correction = 5 * cold * cold / 4;

        if (temperature < -1500) {

            int64_t very_cold = temperature + 1500;

            offset_correction += 7 * very_cold * very_cold;

            sensitivity_correction += 11 * very_cold * very_cold / 2;
        }

        temperature -= temperature_correction;

        offset -= offset_correction;

        sensitivity -= sensitivity_correction;
    }

    int64_t true_pressure = ((int64_t)raw_pressure * sensitivity / 2097152 -
                        offset) / 32768;

    *pressure = (int32_t)true_pressure;

    *centicelsius = (int32_t)temperature;

    return MS5611_OK;
}
