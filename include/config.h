#ifndef CUBESAT_CONFIG_H
#define CUBESAT_CONFIG_H

#if (defined(CUBESAT_STM32H7) + defined(CUBESAT_STM32N6)) != 1
#error "Select only one STM32 family"
#endif

#if defined(CUBESAT_NUCLEO_H753ZI) && !defined(CUBESAT_STM32H7)
#error "NUCLEO-H753ZI requires STM32H7"
#endif

#define SPI_POLL_LIMIT 1000000U
#define SPI_DEMO_ROUNDS 5U

#ifndef I2C_POLL_LIMIT
#define I2C_POLL_LIMIT 1000000U
#endif

#endif
