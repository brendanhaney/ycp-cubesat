// SOURCES:
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf
// https://github.com/boschsensortec/BMI270_SensorAPI/tree/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/bmi2.c
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/bmi2.h
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/bmi2_defs.h
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/bmi270.c
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/bmi270.h
// https://github.com/boschsensortec/BMI270_SensorAPI/blob/d270cdee303b2ed9ea4c43fb7064da8ded8a48b4/LICENSE
// https://docs.sparkfun.com/SparkFun_Qwiic_6DoF_BMI270/hardware_overview/
// https://docs.sparkfun.com/SparkFun_Qwiic_6DoF_BMI270/examples/
// https://www.digikey.com/en/products/detail/sparkfun-electronics/22397/21703804

#ifndef BMI270_DRIVER_H
#define BMI270_DRIVER_H

#include <stdint.h>
#include "bosch/bmi2_defs.h"

enum {
    BMI270_OK = 0,
    BMI270_NOT_READY = 1,
    BMI270_ERR_ID = -10,
    BMI270_ERR_CONFIGURATION = -11,
    BMI270_ERR_NOT_INITIALIZED = -12
};

typedef struct {
    uint8_t address;
    int initialized;
    int i2c_status;
    struct bmi2_dev device;
} bmi270_t;

typedef struct {
    int32_t accel_mg[3];
    int32_t gyro_mdps[3];
} bmi270_sample_t;

int bmi270_init(bmi270_t *sensor, uint8_t address);

int bmi270_read(bmi270_t *sensor, bmi270_sample_t *sample);

#endif
