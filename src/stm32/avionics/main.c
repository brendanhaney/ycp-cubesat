#include <drivers.h>
#include <sensors.h>
#include <utils.h>

static void print_error(const char *sensor, const char *operation, int status, uint8_t address) {

    const char *message;

    switch (status) {

        case I2C_ERR_ARGUMENT: message = "invalid argument"; break;
        case I2C_ERR_TIMEOUT: message = "timeout"; break;
        case I2C_ERR_NACK: message = "no acknowledgement at "; break;
        case I2C_ERR_BUS: message = "I2C bus error"; break;
        case I2C_ERR_NOT_INITIALIZED: message = "I2C not initialized"; break;
        case TMP117_ERR_ID: message = "unexpected device ID (expected TMP117)"; break;
        case MS5611_ERR_CALIBRATION: message = "invalid calibration/CRC"; break;
        case MS5611_ERR_DATA: message = "invalid ADC data"; break;
        case MS5611_ERR_NOT_INITIALIZED: message = "barometer not initialized"; break;
        case BMI270_ERR_ID: message = "unexpected device ID (expected BMI270)"; break;
        case BMI270_ERR_CONFIGURATION: message = "IMU configuration failed"; break;
        case BMI270_ERR_NOT_INITIALIZED: message = "IMU not initialized"; break;
        default: message = "unknown error"; break;

    }

    uart_printf(sensor);
    uart_printf(": ");
    uart_printf(operation);
    uart_printf(" failed: ");

    uart_printf(message);

    if (status == I2C_ERR_NACK)
        uart_printf("%x", address);

    uart_printf("\n");
}

static int poll_thermometer(int *ready) {
    if (!*ready) {

        int status = tmp117_check(THERMOMETER_1_ADDRESS);

        if (status != TMP117_OK) {

            print_error("thermometer_1", "device check", status, THERMOMETER_1_ADDRESS);

            return status;
        }

        *ready = 1;
    }

    int32_t temp;

    int status = tmp117_read(THERMOMETER_1_ADDRESS, &temp);

    if (status == TMP117_OK) {
        
        uart_printf("Temperature: %f C\n", (double)temp / 1000);

    } else if (status == TMP117_NOT_READY) {

        uart_printf("Thermometer: not ready!\n");

    } else {

        print_error("Thermometer", "read", status, THERMOMETER_1_ADDRESS);

        *ready = 0;

    }

    return status;
}

static void print_barometer_init_error(int status, uint8_t address, uint8_t command) {

    const char *operation = command == 0x1eU &&
                            status != MS5611_ERR_CALIBRATION ?
                            "reset" : "calibration read";

    print_error("barometer_1", operation, status, address);

}

static int poll_barometer(ms5611_t *sensor) {

    if (!sensor->initialized) {

        uint8_t address = BAROMETER_1_ADDRESS;

        int status = ms5611_init(sensor, address);

        if (status == I2C_ERR_NACK) {

            uint8_t first_command = sensor->last_command;

            address ^= 1U;

            status = ms5611_init(sensor, address);

            if (status != MS5611_OK)
                print_barometer_init_error(I2C_ERR_NACK, BAROMETER_1_ADDRESS, first_command);
        }

        if (status != MS5611_OK) {

            print_barometer_init_error(status, address, sensor->last_command);

            return status;
        }
    }

    int32_t pressure;
    int32_t temperature;
    int status = ms5611_read(sensor, &pressure, &temperature);

    if (status == MS5611_OK) {

        uart_printf("Pressure: %f hPa\n", (double)pressure / 100);

    } else {

        print_error("barometer_1", "read", status, sensor->address);

        sensor->initialized = 0;

    }

    return status;
}

static int poll_imu(bmi270_t *sensor) {

    if (!sensor->initialized) {

        uint8_t address = IMU_1_ADDRESS;
        int status = bmi270_init(sensor, address);

        if (status == I2C_ERR_NACK || status == BMI270_ERR_ID) {

            int first_status = status;
            address ^= 1U;
            status = bmi270_init(sensor, address);

            if (status != BMI270_OK)
                print_error("imu_1", "initialization", first_status, IMU_1_ADDRESS);
        }

        if (status != BMI270_OK) {

            print_error("imu_1", "initialization", status, address);

            return status;
        }

        uart_printf("imu_1: BMI270 initialized at 0x%x\n", (unsigned int)sensor->address);
    }

    bmi270_sample_t sample;
    int status = bmi270_read(sensor, &sample);

    if (status == BMI270_OK) {

        uart_printf("Accelerometer: X=%f Y=%f Z=%f g\n",
                    (double)sample.accel_mg[0] / 1000,
                    (double)sample.accel_mg[1] / 1000,
                    (double)sample.accel_mg[2] / 1000);
        uart_printf("Gyroscope: X=%f Y=%f Z=%f deg/s\n",
                    (double)sample.gyro_mdps[0] / 1000,
                    (double)sample.gyro_mdps[1] / 1000,
                    (double)sample.gyro_mdps[2] / 1000);

    } else if (status == BMI270_NOT_READY) {

        uart_printf("imu_1: waiting for conversion\n");

    } else {

        print_error("imu_1", "read", status, sensor->address);
        sensor->initialized = 0;
    }

    return status;
}

static void report_address(uint8_t address) {

    int status = i2c2_probe(address);

    uart_printf("I2C2 probe %x", address);

    switch (status) {

        case I2C_OK:            uart_printf(": ACK\n"); break;
        case I2C_ERR_NACK:      uart_printf(": NACK\n"); break;
        case I2C_ERR_TIMEOUT:   uart_printf(": timeout\n"); break;
        case I2C_ERR_BUS:       uart_printf(": bus error\n"); break;
        default:                uart_printf(": error\n"); break;

    }
}

static int bus_failed(int status) {

    return status == I2C_ERR_TIMEOUT || status == I2C_ERR_BUS ||
           status == I2C_ERR_NOT_INITIALIZED;

}

int main(void) {

    int bus_ready = 0;
    int addresses_reported = 0;
    int thermometer_ready = 0;

    ms5611_t barometer = {0};
    bmi270_t imu = {0};

    uart_init();
    uart_printf("\nHello, from the Avionics Board!\n");

    // Core clock: 64 MHz.
    if (delay_init(64000000U)) {

        uart_printf("Avionics: delay initialization failed\n");

        return 1;
    }

    uart_printf("Thermometer addr: %x on I2C\n", THERMOMETER_1_ADDRESS);
    uart_printf("Barometer   addr: %x on I2C\n\n", BAROMETER_1_ADDRESS);
    uart_printf("IMU         addr: 0x%x on I2C (alternate 0x%x)\n\n",
                IMU_1_ADDRESS, IMU_1_ADDRESS ^ 1U);

    for (;;) {
        if (!bus_ready) {

            int status = i2c2_init();

            if (status != I2C_OK) {

                print_error("I2C2", "initialization", status, 0);

            } else {

                bus_ready = 1;

                if (!addresses_reported) {
                    
                    report_address(THERMOMETER_1_ADDRESS);

                    report_address(BAROMETER_1_ADDRESS);
                    report_address(BAROMETER_1_ADDRESS ^ 1U);
                    report_address(IMU_1_ADDRESS);
                    report_address(IMU_1_ADDRESS ^ 1U);

                    addresses_reported = 1;
                }

            }
        }

        if (bus_ready) {

            int thermometer_status = poll_thermometer(&thermometer_ready);

            int barometer_status = poll_barometer(&barometer);
            int imu_status = poll_imu(&imu);

            uart_printf("\n");

            if (bus_failed(thermometer_status) || bus_failed(barometer_status) ||
                bus_failed(imu_status)) {

                bus_ready = 0;

                thermometer_ready = 0;

                barometer.initialized = 0;
                imu.initialized = 0;

            }
        }

        delay_ms(1000U);
    }
}
