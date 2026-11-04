// SOURCES:
// https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
// https://www.st.com/resource/en/errata_sheet/es0392-stm32h742xig-stm32h743xig-stm32h750xb-stm32h753xi-device-errata-stmicroelectronics.pdf
// https://www.st.com/resource/en/datasheet/stm32h753vi.pdf
// https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Src/stm32h7xx_hal_i2c.c
// https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Inc/stm32h7xx_hal_i2c.h

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    I2C_OK = 0,
    I2C_ERR_ARGUMENT = -1,
    I2C_ERR_TIMEOUT = -2,
    I2C_ERR_NACK = -3,
    I2C_ERR_BUS = -4,
    I2C_ERR_NOT_INITIALIZED = -5
} i2c_status_t;

/* I2C2 master on PF1/SCL and PF0/SDA (AF4), standard mode up to 100 kHz.
 * Connect external pull-ups to 3.3 V and a common ground. Blocking, bounded
 * polling only; callers must serialize access, including interrupt handlers.
 */
i2c_status_t i2c2_init(void);
i2c_status_t i2c2_probe(uint8_t address);

/* Pass unshifted 7-bit sensor addresses (0x08..0x77), e.g. 0x48.
 * Each buffer must be non-NULL and each length must be 1..255 bytes.
 * Success means the complete transaction, including STOP, has finished.
 */
i2c_status_t i2c2_write(uint8_t address, const void *data, size_t length);
i2c_status_t i2c2_read(uint8_t address, void *data, size_t length);

/* Write then read with a repeated START, keeping the bus between phases. */
i2c_status_t i2c2_write_read(uint8_t address,
                            const void *tx, size_t tx_length,
                            void *rx, size_t rx_length);

/* Helpers for sensors with an 8-bit register address. The register byte and
 * payload share one write transaction, so write_reg payloads are 1..254 bytes.
 * read_reg uses a repeated START and accepts 1..255 bytes.
 * For wider register addresses, use write_read with the address bytes.
 */
i2c_status_t i2c2_write_reg(uint8_t address, uint8_t reg,
                           const void *data, size_t length);
i2c_status_t i2c2_read_reg(uint8_t address, uint8_t reg,
                          void *data, size_t length);

#endif
