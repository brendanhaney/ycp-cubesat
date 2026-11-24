#ifndef CUBESAT_SENSORS_H
#define CUBESAT_SENSORS_H

/* Unshifted 7-bit sensor addresses. TMP117 ADD0: GND=0x48, V+=0x49. */
#ifndef THERMOMETER_1_ADDRESS
#define THERMOMETER_1_ADDRESS 0x48U
#endif
#ifndef THERMOMETER_2_ADDRESS
#define THERMOMETER_2_ADDRESS 0x49U
#endif
#ifndef BAROMETER_1_ADDRESS
#define BAROMETER_1_ADDRESS 0x77U
#endif
#ifndef IMU_1_ADDRESS
#define IMU_1_ADDRESS 0x68U
#endif

/* Sensor bus polling limit. */
#ifndef I2C_POLL_LIMIT
#define I2C_POLL_LIMIT 1000000U
#endif

#ifndef AVIONICS_BAROMETER_TRY_ALTERNATE_ADDRESS
#define AVIONICS_BAROMETER_TRY_ALTERNATE_ADDRESS 1
#endif
#ifndef AVIONICS_IMU_TRY_ALTERNATE_ADDRESS
#define AVIONICS_IMU_TRY_ALTERNATE_ADDRESS 1
#endif

/* I2C2 TIMINGR presets for each supported HSI divider. Defaults provide
 * conservative Standard-mode operation below 100 kHz. Change all four
 * together when calculating timings for different wiring or bus speeds. */
#ifndef AVIONICS_I2C_TIMING_HSI64
#define AVIONICS_I2C_TIMING_HSI64 0xF0521414U
#endif
#ifndef AVIONICS_I2C_TIMING_HSI32
#define AVIONICS_I2C_TIMING_HSI32 0x70521414U
#endif
#ifndef AVIONICS_I2C_TIMING_HSI16
#define AVIONICS_I2C_TIMING_HSI16 0x30521414U
#endif
#ifndef AVIONICS_I2C_TIMING_HSI8
#define AVIONICS_I2C_TIMING_HSI8 0x10521414U
#endif

/* MS5611 oversampling: 256, 512, 1024, 2048 or 4096.
 * Conversion command and minimum wait change together. */
#ifndef SENSOR_MS5611_OVERSAMPLING
#define SENSOR_MS5611_OVERSAMPLING 4096U
#endif

/* BMI270 full-scale ranges and output data rates. Board sampling in config.h
 * is independent of the sensor's continuous internal output data rate. */
#ifndef SENSOR_BMI270_ACCEL_RANGE_G
#define SENSOR_BMI270_ACCEL_RANGE_G 2U
#endif
#ifndef SENSOR_BMI270_GYRO_RANGE_DPS
#define SENSOR_BMI270_GYRO_RANGE_DPS 250U
#endif
#ifndef SENSOR_BMI270_ACCEL_RATE_HZ
#define SENSOR_BMI270_ACCEL_RATE_HZ 100U
#endif
#ifndef SENSOR_BMI270_GYRO_RATE_HZ
#define SENSOR_BMI270_GYRO_RATE_HZ 100U
#endif
/* Bosch filter bandwidth codes: accelerometer 0..2 in performance mode
 * (0=OSR4, 1=OSR2, 2=normal); gyroscope 0=OSR4, 1=OSR2, 2=normal. */
#ifndef SENSOR_BMI270_ACCEL_BANDWIDTH
#define SENSOR_BMI270_ACCEL_BANDWIDTH 2U
#endif
#ifndef SENSOR_BMI270_GYRO_BANDWIDTH
#define SENSOR_BMI270_GYRO_BANDWIDTH 2U
#endif

#if (AVIONICS_BAROMETER_TRY_ALTERNATE_ADDRESS != 0 && AVIONICS_BAROMETER_TRY_ALTERNATE_ADDRESS != 1) || \
    (AVIONICS_IMU_TRY_ALTERNATE_ADDRESS != 0 && AVIONICS_IMU_TRY_ALTERNATE_ADDRESS != 1)
#error "Alternate sensor address options must be 0 or 1"
#endif
#if SENSOR_MS5611_OVERSAMPLING != 256 && SENSOR_MS5611_OVERSAMPLING != 512 && \
    SENSOR_MS5611_OVERSAMPLING != 1024 && SENSOR_MS5611_OVERSAMPLING != 2048 && \
    SENSOR_MS5611_OVERSAMPLING != 4096
#error "MS5611 oversampling must be 256, 512, 1024, 2048 or 4096"
#endif
#if SENSOR_BMI270_ACCEL_RANGE_G != 2 && SENSOR_BMI270_ACCEL_RANGE_G != 4 && \
    SENSOR_BMI270_ACCEL_RANGE_G != 8 && SENSOR_BMI270_ACCEL_RANGE_G != 16
#error "BMI270 accelerometer range must be 2, 4, 8 or 16 g"
#endif
#if SENSOR_BMI270_GYRO_RANGE_DPS != 125 && SENSOR_BMI270_GYRO_RANGE_DPS != 250 && \
    SENSOR_BMI270_GYRO_RANGE_DPS != 500 && SENSOR_BMI270_GYRO_RANGE_DPS != 1000 && \
    SENSOR_BMI270_GYRO_RANGE_DPS != 2000
#error "BMI270 gyroscope range must be 125, 250, 500, 1000 or 2000 deg/s"
#endif
#if SENSOR_BMI270_ACCEL_RATE_HZ != 25 && SENSOR_BMI270_ACCEL_RATE_HZ != 50 && \
    SENSOR_BMI270_ACCEL_RATE_HZ != 100 && SENSOR_BMI270_ACCEL_RATE_HZ != 200 && \
    SENSOR_BMI270_ACCEL_RATE_HZ != 400 && SENSOR_BMI270_ACCEL_RATE_HZ != 800 && \
    SENSOR_BMI270_ACCEL_RATE_HZ != 1600
#error "BMI270 accelerometer rate must be 25, 50, 100, 200, 400, 800 or 1600 Hz"
#endif
#if SENSOR_BMI270_GYRO_RATE_HZ != 25 && SENSOR_BMI270_GYRO_RATE_HZ != 50 && \
    SENSOR_BMI270_GYRO_RATE_HZ != 100 && SENSOR_BMI270_GYRO_RATE_HZ != 200 && \
    SENSOR_BMI270_GYRO_RATE_HZ != 400 && SENSOR_BMI270_GYRO_RATE_HZ != 800 && \
    SENSOR_BMI270_GYRO_RATE_HZ != 1600 && SENSOR_BMI270_GYRO_RATE_HZ != 3200
#error "BMI270 gyroscope rate must be 25, 50, 100, 200, 400, 800, 1600 or 3200 Hz"
#endif
#if SENSOR_BMI270_ACCEL_BANDWIDTH < 0 || SENSOR_BMI270_ACCEL_BANDWIDTH > 2 || \
    SENSOR_BMI270_GYRO_BANDWIDTH < 0 || SENSOR_BMI270_GYRO_BANDWIDTH > 2
#error "BMI270 performance-mode bandwidth codes must be 0, 1 or 2"
#endif

#if THERMOMETER_1_ADDRESS < 0x08 || THERMOMETER_1_ADDRESS > 0x77 || \
    THERMOMETER_2_ADDRESS < 0x08 || THERMOMETER_2_ADDRESS > 0x77 || \
    BAROMETER_1_ADDRESS < 0x08 || BAROMETER_1_ADDRESS > 0x77 || \
    IMU_1_ADDRESS < 0x08 || IMU_1_ADDRESS > 0x77 || \
    THERMOMETER_1_ADDRESS == THERMOMETER_2_ADDRESS
#error "Sensor addresses must be 7-bit and the two thermometers need distinct addresses"
#endif
#if I2C_POLL_LIMIT < 1
#error "I2C poll limit must be nonzero"
#endif

#endif
