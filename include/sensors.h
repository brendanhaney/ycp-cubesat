#ifndef CUBESAT_SENSORS_H
#define CUBESAT_SENSORS_H

// Unshifted 7-bit I2C addresses. TMP117 ADD0 must differ between boards:
// GND selects 0x48; V+ selects 0x49 (see sources in drivers/tmp117.h).
#define THERMOMETER_1_ADDRESS 0x48U
#define THERMOMETER_2_ADDRESS 0x49U
#define BAROMETER_1_ADDRESS 0x77U
#define IMU_1_ADDRESS 0x68U

#endif
