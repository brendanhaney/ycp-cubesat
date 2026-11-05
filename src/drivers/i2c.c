#include <config.h>
#include <registers.h>

#include "i2c.h"

#if defined(CUBESAT_STM32H7)

/* Host tests replace MMIO access with a reactive peripheral model. */
#ifdef I2C_TEST
#include "i2c_test_io.h"
#else
#define I2C_READ(reg) (reg)
#define I2C_WRITE(reg, value) ((reg) = (value))
#endif

#define CR1_PE       (1U << 0)
#define CR2_RD_WRN   (1U << 10)
#define CR2_START    (1U << 13)
#define CR2_STOP     (1U << 14)
#define CR2_RELOAD   (1U << 24)
#define CR2_AUTOEND  (1U << 25)
#define ISR_TXIS     (1U << 1)
#define ISR_RXNE     (1U << 2)
#define ISR_NACKF    (1U << 4)
#define ISR_STOPF    (1U << 5)
#define ISR_TC       (1U << 6)
#define ISR_BERR     (1U << 8)
#define ISR_ARLO     (1U << 9)
#define ISR_OVR      (1U << 10)
#define ISR_TIMEOUT  (1U << 12)
#define ISR_BUSY     (1U << 15)
#define ICR_CLEAR    0x3F30U

static int initialized;

static i2c_status_t configure_i2c2(uint32_t *timing)
{
    /* HSI /1,/2,/4,/8 with a constant 250 ns timing prescaler period.
     * SCLDEL=1.5 us, SDADEL=500 ns, programmed SCL high/low=5.25 us.
     * Analog filtering and synchronization add margin below 100 kHz.
     */
    static const uint32_t timings[] = {
        0xF0521414U, 0x70521414U, 0x30521414U, 0x10521414U
    };
    uint32_t remaining = I2C_POLL_LIMIT;

    I2C_WRITE(STM32H7_RCC_CR, I2C_READ(STM32H7_RCC_CR) | 1U);
    while (!(I2C_READ(STM32H7_RCC_CR) & (1U << 2))) {
        if (!remaining--)
            return I2C_ERR_TIMEOUT;
    }
    /* I2C123 kernel = HSI. Leave the system/APB clocks and HSIDIV intact. */
    I2C_WRITE(STM32H7_RCC_D2CCIP2R,
              (I2C_READ(STM32H7_RCC_D2CCIP2R) & ~(3U << 12)) | (2U << 12));
    I2C_WRITE(STM32H7_RCC_AHB4ENR, I2C_READ(STM32H7_RCC_AHB4ENR) | (1U << 5));
    I2C_WRITE(STM32H7_RCC_APB1LENR, I2C_READ(STM32H7_RCC_APB1LENR) | (1U << 22));
    (void)I2C_READ(STM32H7_RCC_AHB4ENR);
    (void)I2C_READ(STM32H7_RCC_APB1LENR);
    I2C_WRITE(STM32H7_RCC_APB1LRSTR, I2C_READ(STM32H7_RCC_APB1LRSTR) | (1U << 22));
    I2C_WRITE(STM32H7_RCC_APB1LRSTR, I2C_READ(STM32H7_RCC_APB1LRSTR) & ~(1U << 22));

    /* PF1=SCL, PF0=SDA: AF4, open drain, medium speed, external pull-ups. */
    I2C_WRITE(GPIO_AFRL(STM32H7_GPIOF_BASE),
              (I2C_READ(GPIO_AFRL(STM32H7_GPIOF_BASE)) & ~0xFFU) | 0x44U);
    I2C_WRITE(GPIO_OTYPER(STM32H7_GPIOF_BASE),
              I2C_READ(GPIO_OTYPER(STM32H7_GPIOF_BASE)) | 0x3U);
    I2C_WRITE(GPIO_OSPEEDR(STM32H7_GPIOF_BASE),
              (I2C_READ(GPIO_OSPEEDR(STM32H7_GPIOF_BASE)) & ~0xFU) | 0x5U);
    I2C_WRITE(GPIO_PUPDR(STM32H7_GPIOF_BASE),
              I2C_READ(GPIO_PUPDR(STM32H7_GPIOF_BASE)) & ~0xFU);
    I2C_WRITE(GPIO_MODER(STM32H7_GPIOF_BASE),
              (I2C_READ(GPIO_MODER(STM32H7_GPIOF_BASE)) & ~0xFU) | 0xAU);

    *timing = timings[(I2C_READ(STM32H7_RCC_CR) >> 3) & 3U];
    return I2C_OK;
}

static i2c_status_t wait_flag(uint32_t flag)
{
    for (uint32_t remaining = I2C_POLL_LIMIT; remaining; remaining--) {
        uint32_t status = I2C_READ(I2C_ISR);

        /* ES0392: master-mode BERR may be spurious. Clear it and continue;
         * a transfer that cannot progress still reaches the polling limit.
         */
        if (status & ISR_BERR)
            I2C_WRITE(I2C_ICR, ISR_BERR);
        if (status & ISR_TIMEOUT)
            return I2C_ERR_TIMEOUT;
        if (status & (ISR_ARLO | ISR_OVR))
            return I2C_ERR_BUS;
        if (status & ISR_NACKF)
            return I2C_ERR_NACK;
        if (status & flag)
            return I2C_OK;
        if (status & ISR_STOPF)
            return I2C_ERR_BUS;
    }
    return I2C_ERR_TIMEOUT;
}

static i2c_status_t prepare(void)
{
    if (!initialized || !(I2C_READ(I2C_CR1) & CR1_PE))
        return I2C_ERR_NOT_INITIALIZED;

    I2C_WRITE(I2C_ICR, ICR_CLEAR);
    /* BUSY can belong to another master: wait without requesting STOP. */
    for (uint32_t remaining = I2C_POLL_LIMIT; remaining; remaining--) {
        if (!(I2C_READ(I2C_ISR) & ISR_BUSY))
            return I2C_OK;
    }
    return I2C_ERR_TIMEOUT;
}

static void abort_transfer(void)
{
    uint32_t status = I2C_READ(I2C_ISR);

    /* Called only after our START. STOPF means we already released the bus;
     * ARLO means another master owns it. Neither case permits another STOP.
     */
    if ((status & ISR_BUSY) && !(status & (ISR_ARLO | ISR_STOPF))) {
        uint32_t control = I2C_READ(I2C_CR2);
        control &= ~(CR2_START | CR2_AUTOEND | CR2_RELOAD);
        I2C_WRITE(I2C_CR2, control | CR2_STOP);
        for (uint32_t remaining = I2C_POLL_LIMIT; remaining; remaining--) {
            status = I2C_READ(I2C_ISR);
            if ((status & (ISR_STOPF | ISR_ARLO)) || !(status & ISR_BUSY))
                break;
        }
    }

    /* RM0433 software reset: PE=0 resets pending data and internal state.
     * Readback and subsequent MMIO accesses provide the required APB delay.
     * Preserve timing and the shared RCC clock source, even on a stuck bus.
     */
    I2C_WRITE(I2C_CR1, I2C_READ(I2C_CR1) & ~CR1_PE);
    (void)I2C_READ(I2C_CR1);
    I2C_WRITE(I2C_CR2, 0);
    I2C_WRITE(I2C_ICR, ICR_CLEAR);
    I2C_WRITE(I2C_CR1, I2C_READ(I2C_CR1) | CR1_PE);
}

i2c_status_t i2c2_init(void)
{
    uint32_t timing;
    initialized = 0;
    i2c_status_t result = configure_i2c2(&timing);
    if (result != I2C_OK)
        return result;

    I2C_WRITE(I2C_CR1, 0); /* Analog filter enabled, digital filter disabled. */
    I2C_WRITE(I2C_CR2, 0); /* 7-bit addressing, no reload. */
    I2C_WRITE(I2C_TIMINGR, timing);
    I2C_WRITE(I2C_ICR, ICR_CLEAR);
    I2C_WRITE(I2C_CR1, CR1_PE);
    initialized = 1;
    return I2C_OK;
}

i2c_status_t i2c2_probe(uint8_t address)
{
    if (address < 0x08U || address > 0x77U)
        return I2C_ERR_ARGUMENT;

    i2c_status_t result = prepare();
    if (result != I2C_OK)
        return result;

    I2C_WRITE(I2C_CR2, ((uint32_t)address << 1) | CR2_START | CR2_AUTOEND);
    result = wait_flag(ISR_STOPF);
    if (result != I2C_OK) {
        abort_transfer();
    } else {
        I2C_WRITE(I2C_ICR, ICR_CLEAR);
        I2C_WRITE(I2C_CR2, 0);
    }
    return result;
}

static i2c_status_t write_phase(uint8_t address, const uint8_t *data,
                                size_t length, int prefix, uint8_t reg,
                                int autoend)
{
    size_t count = length + (prefix ? 1U : 0U);
    size_t index = prefix ? 0U : 1U;

    /* ES0392: preload the first byte before START to avoid first-byte stalls
     * at affected ratios between the APB and I2C kernel clocks.
     */
    I2C_WRITE(I2C_TXDR, prefix ? reg : data[0]);
    I2C_WRITE(I2C_CR2, ((uint32_t)address << 1) | ((uint32_t)count << 16) |
              CR2_START | (autoend ? CR2_AUTOEND : 0U));
    for (; index < length; index++) {
        i2c_status_t result = wait_flag(ISR_TXIS);
        if (result != I2C_OK)
            return result;
        I2C_WRITE(I2C_TXDR, data[index]);
    }
    return wait_flag(autoend ? ISR_STOPF : ISR_TC);
}

static i2c_status_t read_phase(uint8_t address, uint8_t *data, size_t length)
{
    I2C_WRITE(I2C_CR2, ((uint32_t)address << 1) | ((uint32_t)length << 16) |
              CR2_RD_WRN | CR2_START | CR2_AUTOEND);
    for (size_t index = 0; index < length; index++) {
        i2c_status_t result = wait_flag(ISR_RXNE);
        if (result != I2C_OK)
            return result;
        data[index] = (uint8_t)I2C_READ(I2C_RXDR);
    }
    return wait_flag(ISR_STOPF);
}

static i2c_status_t transfer(uint8_t address,
                             const uint8_t *tx, size_t tx_length,
                             uint8_t *rx, size_t rx_length,
                             int prefix, uint8_t reg)
{
    i2c_status_t result = prepare();
    if (result != I2C_OK)
        return result;
    if (tx_length || prefix)
        result = write_phase(address, tx, tx_length, prefix, reg, !rx_length);
    if (result == I2C_OK && rx_length)
        result = read_phase(address, rx, rx_length);
    if (result != I2C_OK) {
        abort_transfer();
    } else {
        I2C_WRITE(I2C_ICR, ICR_CLEAR);
        I2C_WRITE(I2C_CR2, 0);
    }
    return result;
}

static int valid(uint8_t address, const void *data, size_t length, size_t max)
{
    return address >= 0x08U && address <= 0x77U && data &&
           length && length <= max;
}

i2c_status_t i2c2_write(uint8_t address, const void *data, size_t length)
{
    if (!valid(address, data, length, 255U))
        return I2C_ERR_ARGUMENT;
    return transfer(address, data, length, NULL, 0, 0, 0);
}

i2c_status_t i2c2_read(uint8_t address, void *data, size_t length)
{
    if (!valid(address, data, length, 255U))
        return I2C_ERR_ARGUMENT;
    return transfer(address, NULL, 0, data, length, 0, 0);
}

i2c_status_t i2c2_write_read(uint8_t address,
                            const void *tx, size_t tx_length,
                            void *rx, size_t rx_length)
{
    if (!valid(address, tx, tx_length, 255U) ||
        !valid(address, rx, rx_length, 255U))
        return I2C_ERR_ARGUMENT;
    return transfer(address, tx, tx_length, rx, rx_length, 0, 0);
}

i2c_status_t i2c2_write_reg(uint8_t address, uint8_t reg,
                           const void *data, size_t length)
{
    if (!valid(address, data, length, 254U))
        return I2C_ERR_ARGUMENT;
    return transfer(address, data, length, NULL, 0, 1, reg);
}

i2c_status_t i2c2_read_reg(uint8_t address, uint8_t reg,
                          void *data, size_t length)
{
    if (!valid(address, data, length, 255U))
        return I2C_ERR_ARGUMENT;
    return transfer(address, NULL, 0, data, length, 1, reg);
}

#else
#error "I2C2 driver is only implemented for STM32H7"
#endif
