#include <sensors.h>
#include "bmi270.h"
#include "bosch/bmi270.h"
#include "i2c.h"
#include "utils/delay.h"

#define BMI270_TRANSFER_LIMIT 254u
#define BMI270_CONFIG_CHUNK 128u
#define BMI270_ERR_REG 0x02u
#define BMI270_INTERNAL_ERRORS 0x60u
#define BMI270_SENSOR_ERRORS 0xdfu
#define BMI270_BOTH_READY (BMI2_DRDY_ACC | BMI2_DRDY_GYR)
#define BMI270_BOTH_ENABLED (BMI2_ACC_EN_MASK | BMI2_GYR_EN_MASK)

#if SENSOR_BMI270_ACCEL_RANGE_G == 2
#define BMI270_ACCEL_RANGE BMI2_ACC_RANGE_2G
#elif SENSOR_BMI270_ACCEL_RANGE_G == 4
#define BMI270_ACCEL_RANGE BMI2_ACC_RANGE_4G
#elif SENSOR_BMI270_ACCEL_RANGE_G == 8
#define BMI270_ACCEL_RANGE BMI2_ACC_RANGE_8G
#else
#define BMI270_ACCEL_RANGE BMI2_ACC_RANGE_16G
#endif

#if SENSOR_BMI270_GYRO_RANGE_DPS == 125
#define BMI270_GYRO_RANGE BMI2_GYR_RANGE_125
#elif SENSOR_BMI270_GYRO_RANGE_DPS == 250
#define BMI270_GYRO_RANGE BMI2_GYR_RANGE_250
#elif SENSOR_BMI270_GYRO_RANGE_DPS == 500
#define BMI270_GYRO_RANGE BMI2_GYR_RANGE_500
#elif SENSOR_BMI270_GYRO_RANGE_DPS == 1000
#define BMI270_GYRO_RANGE BMI2_GYR_RANGE_1000
#else
#define BMI270_GYRO_RANGE BMI2_GYR_RANGE_2000
#endif

/* Both sensors encode the integer power-of-two rates from 25 Hz as 6..12. */
#define BMI270_RATE_CODE(hz) \
    ((hz) == 25U ? 6U : (hz) == 50U ? 7U : (hz) == 100U ? 8U : \
     (hz) == 200U ? 9U : (hz) == 400U ? 10U : (hz) == 800U ? 11U : \
     (hz) == 1600U ? 12U : 13U)
#define BMI270_ACCEL_ODR BMI270_RATE_CODE(SENSOR_BMI270_ACCEL_RATE_HZ)
#define BMI270_GYRO_ODR BMI270_RATE_CODE(SENSOR_BMI270_GYRO_RATE_HZ)

/* The vendor API collapses transport errors to BMI2_E_COM_FAIL. Retain the
 * first bus error for the public API and stop subsequent transfers if an
 * upstream operation continues after a failed callback.
 */
static BMI2_INTF_RETURN_TYPE bus_result(bmi270_t *sensor, int status) {

    if (sensor->i2c_status == I2C_OK && status != I2C_OK)
        sensor->i2c_status = status;

    return (BMI2_INTF_RETURN_TYPE)sensor->i2c_status;
}

static BMI2_INTF_RETURN_TYPE read_register(uint8_t reg, uint8_t *data,
                                          uint32_t length, void *context) {

    bmi270_t *sensor = context;

    if (sensor->i2c_status != I2C_OK)
        return (BMI2_INTF_RETURN_TYPE)sensor->i2c_status;

    if (data == NULL || length == 0 || length > BMI270_TRANSFER_LIMIT)
        return bus_result(sensor, I2C_ERR_ARGUMENT);

    return bus_result(sensor,
                      i2c2_read_reg(sensor->address, reg, data, length));
}

static BMI2_INTF_RETURN_TYPE write_register(uint8_t reg, const uint8_t *data,
                                           uint32_t length, void *context) {

    bmi270_t *sensor = context;

    if (sensor->i2c_status != I2C_OK)
        return (BMI2_INTF_RETURN_TYPE)sensor->i2c_status;

    if (data == NULL || length == 0 || length > BMI270_TRANSFER_LIMIT)
        return bus_result(sensor, I2C_ERR_ARGUMENT);

    return bus_result(sensor,
                      i2c2_write_reg(sensor->address, reg, data, length));
}

static void wait_us(uint32_t microseconds, void *context) {

    (void)context;
    /* Round up so Bosch's minimum delays are respected by our ms timer. */
    delay_ms(microseconds / 1000u + (microseconds % 1000u != 0u));
}

static int api_result(const bmi270_t *sensor, int8_t result) {

    if (sensor->i2c_status != I2C_OK)
        return sensor->i2c_status;

    if (result == BMI2_E_DEV_NOT_FOUND)
        return BMI270_ERR_ID;

    return result == BMI2_OK ? BMI270_OK : BMI270_ERR_CONFIGURATION;
}

static int verify_configuration(bmi270_t *sensor) {

    struct bmi2_sens_config config[2] = {
        { .type = BMI2_ACCEL },
        { .type = BMI2_GYRO }
    };
    uint8_t enabled;
    uint8_t internal_status;
    uint8_t errors;

    int status = api_result(sensor,
                            bmi270_get_sensor_config(config, 2, &sensor->device));

    if (status != BMI270_OK)
        return status;

    if (config[0].cfg.acc.odr != BMI270_ACCEL_ODR ||
        config[0].cfg.acc.range != BMI270_ACCEL_RANGE ||
        config[0].cfg.acc.bwp != SENSOR_BMI270_ACCEL_BANDWIDTH ||
        config[0].cfg.acc.filter_perf != BMI2_PERF_OPT_MODE ||
        config[1].cfg.gyr.odr != BMI270_GYRO_ODR ||
        config[1].cfg.gyr.range != BMI270_GYRO_RANGE ||
        config[1].cfg.gyr.bwp != SENSOR_BMI270_GYRO_BANDWIDTH ||
        config[1].cfg.gyr.filter_perf != BMI2_PERF_OPT_MODE ||
        config[1].cfg.gyr.noise_perf != BMI2_PERF_OPT_MODE ||
        config[1].cfg.gyr.ois_range != BMI2_GYR_OIS_250)
        return BMI270_ERR_CONFIGURATION;

    status = api_result(sensor, bmi2_get_regs(BMI2_PWR_CTRL_ADDR, &enabled,
                                              1, &sensor->device));

    if (status != BMI270_OK)
        return status;

    if ((enabled & BMI270_BOTH_ENABLED) != BMI270_BOTH_ENABLED)
        return BMI270_ERR_CONFIGURATION;

    status = api_result(sensor, bmi2_get_internal_status(&internal_status,
                                                         &sensor->device));

    if (status != BMI270_OK)
        return status;

    if ((internal_status & BMI2_CONFIG_LOAD_STATUS_MASK) != BMI2_CONFIG_LOAD_SUCCESS ||
        (internal_status & BMI270_INTERNAL_ERRORS) != 0)
        return BMI270_ERR_CONFIGURATION;

    status = api_result(sensor, bmi2_get_regs(BMI270_ERR_REG, &errors,
                                              1, &sensor->device));

    if (status != BMI270_OK)
        return status;

    return (errors & BMI270_SENSOR_ERRORS) ? BMI270_ERR_CONFIGURATION : BMI270_OK;
}

int bmi270_init(bmi270_t *sensor, uint8_t address) {

    if (sensor == NULL)
        return I2C_ERR_ARGUMENT;

    *sensor = (bmi270_t){ .address = address };

    if (address != BMI2_I2C_PRIM_ADDR && address != BMI2_I2C_SEC_ADDR) {
        sensor->i2c_status = I2C_ERR_ARGUMENT;
        return I2C_ERR_ARGUMENT;
    }

    sensor->device = (struct bmi2_dev){
        .intf = BMI2_I2C_INTF,
        .intf_ptr = sensor,
        .read = read_register,
        .write = write_register,
        .delay_us = wait_us,
        .read_write_len = BMI270_CONFIG_CHUNK
    };

    /* The official init validates CHIP_ID before soft reset, uploads the
     * complete configuration image and checks INTERNAL_STATUS.init_ok.
     */
    int status = api_result(sensor, bosch_bmi270_init(&sensor->device));

    if (status != BMI270_OK)
        return status;

    /* Continuous sampling and burst transfers use normal/performance mode. */
    status = api_result(sensor,
                         bmi2_set_adv_power_save(BMI2_DISABLE, &sensor->device));

    if (status != BMI270_OK)
        return status;

    struct bmi2_sens_config config[2] = {
        {
            .type = BMI2_ACCEL,
            .cfg.acc = {
                .odr = BMI270_ACCEL_ODR,
                .bwp = SENSOR_BMI270_ACCEL_BANDWIDTH,
                .filter_perf = BMI2_PERF_OPT_MODE,
                .range = BMI270_ACCEL_RANGE
            }
        },
        {
            .type = BMI2_GYRO,
            .cfg.gyr = {
                .odr = BMI270_GYRO_ODR,
                .bwp = SENSOR_BMI270_GYRO_BANDWIDTH,
                .filter_perf = BMI2_PERF_OPT_MODE,
                .ois_range = BMI2_GYR_OIS_250,
                .range = BMI270_GYRO_RANGE,
                .noise_perf = BMI2_PERF_OPT_MODE
            }
        }
    };

    for (unsigned int i = 0; i < 2; ++i) {
        status = api_result(sensor,
                             bmi270_set_sensor_config(&config[i], 1, &sensor->device));

        if (status != BMI270_OK)
            return status;
    }

    const uint8_t sensors[] = { BMI2_ACCEL, BMI2_GYRO };

    status = api_result(sensor,
                         bmi270_sensor_enable(sensors, 2, &sensor->device));

    if (status != BMI270_OK)
        return status;

    status = verify_configuration(sensor);

    if (status == BMI270_OK)
        sensor->initialized = 1;

    return status;
}

int bmi270_read(bmi270_t *sensor, bmi270_sample_t *sample) {

    if (sensor == NULL || sample == NULL)
        return I2C_ERR_ARGUMENT;

    if (!sensor->initialized)
        return BMI270_ERR_NOT_INITIALIZED;

    sensor->i2c_status = I2C_OK;

    uint8_t ready;
    int status = api_result(sensor, bmi2_get_status(&ready, &sensor->device));

    if (status != BMI270_OK)
        return status;

    if ((ready & BMI270_BOTH_READY) != BMI270_BOTH_READY)
        return BMI270_NOT_READY;

    /* STATUS and all axes are acquired in one burst. The vendor decoder also
     * applies Bosch's factory gyroscope cross-axis sensitivity correction.
     */
    struct bmi2_sens_data data;

    status = api_result(sensor, bmi2_get_sensor_data(&data, &sensor->device));

    if (status != BMI270_OK)
        return status;

    if ((data.status & BMI270_BOTH_READY) != BMI270_BOTH_READY)
        return BMI270_NOT_READY;

    /* Scale using the same full-scale ranges written above. The gyro product
     * needs 64 bits at the larger ranges; no floating point is required. */
    bmi270_sample_t result = {
        .accel_mg = {
            (int32_t)data.acc.x * (int32_t)(SENSOR_BMI270_ACCEL_RANGE_G * 1000U) / 32768,
            (int32_t)data.acc.y * (int32_t)(SENSOR_BMI270_ACCEL_RANGE_G * 1000U) / 32768,
            (int32_t)data.acc.z * (int32_t)(SENSOR_BMI270_ACCEL_RANGE_G * 1000U) / 32768
        },
        .gyro_mdps = {
            (int64_t)data.gyr.x * (SENSOR_BMI270_GYRO_RANGE_DPS * 1000) / 32768,
            (int64_t)data.gyr.y * (SENSOR_BMI270_GYRO_RANGE_DPS * 1000) / 32768,
            (int64_t)data.gyr.z * (SENSOR_BMI270_GYRO_RANGE_DPS * 1000) / 32768
        }
    };

    *sample = result;
    return BMI270_OK;
}
