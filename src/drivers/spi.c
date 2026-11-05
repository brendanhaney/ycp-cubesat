#include <config.h>
#include <registers.h>

#include "spi.h"
#if defined(CUBESAT_STM32N6)
#include "gpio.h"
#endif

static int is_master;

static int wait_status(uint32_t mask) {
    uint32_t remaining = SPI_POLL_LIMIT;

    while (!(SPI_SR & mask)) {
        if ((SPI_SR & SPI_SR_ERRORS) || !--remaining) {
            return -1;
        }
    }
    return (SPI_SR & SPI_SR_ERRORS) ? -1 : 0;
}

static int wait_pin(uintptr_t port, uint32_t mask, int high) {
    uint32_t remaining = SPI_POLL_LIMIT;

    while (!!(GPIO_IDR(port) & mask) != high) {
        if (!--remaining) {
            return -1;
        }
    }
    return 0;
}

int spi4_init(int master) {
#if defined(CUBESAT_STM32H7)
    uint32_t remaining = SPI_POLL_LIMIT;

    STM32H7_RCC_CR |= 1U;
    while (!(STM32H7_RCC_CR & (1U << 2))) {
        if (!--remaining) {
            return -1;
        }
    }
    STM32H7_RCC_D2CCIP1R = (STM32H7_RCC_D2CCIP1R & ~(7U << 16)) | (3U << 16);
    STM32H7_RCC_AHB4ENR |= (1U << 1) | (1U << 4);
    STM32H7_RCC_APB2ENR |= 1U << 13;
    (void)STM32H7_RCC_APB2ENR;

    GPIO_MODER(STM32H7_GPIOE_BASE) = (GPIO_MODER(STM32H7_GPIOE_BASE) & ~0x3C30U) | 0x2820U;
    GPIO_AFRL(STM32H7_GPIOE_BASE) = (GPIO_AFRL(STM32H7_GPIOE_BASE) & ~0x0FF00F00U) | 0x05500500U;
    GPIO_OTYPER(STM32H7_GPIOE_BASE) &= ~0x64U;
    GPIO_OSPEEDR(STM32H7_GPIOE_BASE) = (GPIO_OSPEEDR(STM32H7_GPIOE_BASE) & ~0x3C30U) | 0x1410U;
    GPIO_PUPDR(STM32H7_GPIOE_BASE) &= ~0x3C30U;

    if (master) {
        GPIO_BSRR(STM32H7_GPIOE_BASE) = (1U << 3) | (1U << 4);
        GPIO_MODER(STM32H7_GPIOE_BASE) = (GPIO_MODER(STM32H7_GPIOE_BASE) & ~0x3C0U) | 0x140U;
        GPIO_OTYPER(STM32H7_GPIOE_BASE) &= ~0x18U;
        GPIO_PUPDR(STM32H7_GPIOE_BASE) &= ~0x3C0U;
        GPIO_MODER(STM32H7_GPIOB_BASE) &= ~0xFU;
        GPIO_PUPDR(STM32H7_GPIOB_BASE) = (GPIO_PUPDR(STM32H7_GPIOB_BASE) & ~0xFU) | 0xAU;
    } else {
        GPIO_MODER(STM32H7_GPIOE_BASE) = (GPIO_MODER(STM32H7_GPIOE_BASE) & ~0x300U) | 0x200U;
        GPIO_AFRL(STM32H7_GPIOE_BASE) = (GPIO_AFRL(STM32H7_GPIOE_BASE) & ~0xF0000U) | 0x50000U;
        GPIO_PUPDR(STM32H7_GPIOE_BASE) = (GPIO_PUPDR(STM32H7_GPIOE_BASE) & ~0x300U) | 0x100U;
        GPIO_BSRR(STM32H7_GPIOB_BASE) = 1U << 16;
        GPIO_MODER(STM32H7_GPIOB_BASE) = (GPIO_MODER(STM32H7_GPIOB_BASE) & ~3U) | 1U;
        GPIO_OTYPER(STM32H7_GPIOB_BASE) &= ~1U;
        GPIO_PUPDR(STM32H7_GPIOB_BASE) &= ~3U;
    }
#elif defined(CUBESAT_STM32N6)
    if (master)
        return -1;
    uint32_t remaining = SPI_POLL_LIMIT;
    STM32N6_RCC_CR |= 1U << 3;
    for (; remaining; remaining--) {
        if (STM32N6_RCC_SR & (1U << 3))
            break;
    }
    if (!remaining)
        return -1;
    STM32N6_RCC_BUSENSR = (1U << 6) | (1U << 9);
    (void)STM32N6_RCC_BUSENR;
    STM32N6_RCC_AHB4ENSR = (1U << 18) | (1U << 4) | (1U << 1);
    (void)STM32N6_RCC_AHB4ENR;
    STM32N6_PWR_SVMCR3 &= ~(1U << 24);
    STM32N6_RCC_CCIPR9 = (STM32N6_RCC_CCIPR9 & ~(7U << 16)) | (5U << 16);
    STM32N6_RCC_APB2ENSR = 1U << 13;
    (void)STM32N6_RCC_APB2ENR;
    STM32N6_RCC_APB2RSTSR = 1U << 13;
    STM32N6_RCC_APB2RSTCR = 1U << 13;

    gpio_alternate(SPI_SLAVE_NSS_PORT, SPI_SLAVE_NSS_PIN, 5U, 1U);
    gpio_alternate(GPIOE_BASE, 12U, 5U, 0U);
    gpio_alternate(GPIOE_BASE, 13U, 5U, 0U);
    gpio_alternate(GPIOE_BASE, 14U, 5U, 0U);

    unsigned int pin = SPI_SLAVE_READY_PIN;
    unsigned int shift = pin * 2U;
    GPIO_BSRR(SPI_SLAVE_READY_PORT) = 1U << (pin + 16U);
    GPIO_OTYPER(SPI_SLAVE_READY_PORT) &= ~(1U << pin);
    GPIO_OSPEEDR(SPI_SLAVE_READY_PORT) =
        (GPIO_OSPEEDR(SPI_SLAVE_READY_PORT) & ~(3U << shift)) | (1U << shift);
    GPIO_PUPDR(SPI_SLAVE_READY_PORT) &= ~(3U << shift);
    GPIO_MODER(SPI_SLAVE_READY_PORT) =
        (GPIO_MODER(SPI_SLAVE_READY_PORT) & ~(3U << shift)) | (1U << shift);
#endif
    is_master = !!master;

    SPI_CR1 = 0;
    SPI_CFG1 = 7U | (7U << 28);
    SPI_CFG2 = SPI_CFG2_AFCNTR |
        (is_master ? SPI_CFG2_MASTER | SPI_CFG2_SSM : 0);
    if (is_master) {
        SPI_CR1 = SPI_CR1_SSI;
    }
    return 0;
}

static int prepare(const void *tx, void *rx, size_t len) {
    const uint8_t *bytes = tx;

    if (!tx || !rx || !len || len > SPI_MAX_TRANSFER) {
        return -1;
    }
    SPI_CR1 = is_master ? SPI_CR1_SSI : 0;
    SPI_IFCR = 0xFF8U;
    SPI_CR2 = len;
    SPI_CR1 |= SPI_CR1_SPE;
    for (size_t i = 0; i < len; i++) {
        if (wait_status(SPI_SR_TXP)) {
            SPI_CR1 &= ~SPI_CR1_SPE;
            return -1;
        }
        SPI_TXDR = bytes[i];
    }
    return 0;
}

static int receive(void *rx, size_t len) {
    uint8_t *bytes = rx;

    for (size_t i = 0; i < len; i++) {
        if (wait_status(SPI_SR_RXP)) {
            return -1;
        }
        bytes[i] = SPI_RXDR;
    }
    return wait_status(SPI_SR_EOT);
}

int spi4_master_transfer(unsigned int slave, const void *tx, void *rx, size_t len) {
#if SPI_MASTER_SUPPORTED
    int result;
    uint32_t cs;

    if (!is_master || slave > SPI_PAYLOAD) {
        return -1;
    }
    cs = SPI_MASTER_CS_MASK(slave);
    if (wait_pin(SPI_MASTER_READY_PORT, SPI_MASTER_READY_MASK(slave), 1) || prepare(tx, rx, len)) {
        return -1;
    }
    GPIO_BSRR(SPI_MASTER_CS_PORT) = cs << 16;
    SPI_CR1 |= SPI_CR1_CSTART;
    result = receive(rx, len);
    // Slave acknowledges completion before CS rises; it cannot rearm early.
    if (!result) {
        result = wait_pin(SPI_MASTER_READY_PORT, SPI_MASTER_READY_MASK(slave), 0);
    }
    GPIO_BSRR(SPI_MASTER_CS_PORT) = cs;
    SPI_CR1 &= ~SPI_CR1_SPE;
    return result;
#else
    (void)slave;
    (void)tx;
    (void)rx;
    (void)len;
    return -1;
#endif
}

int spi4_slave_transfer(const void *tx, void *rx, size_t len) {
    int result;

    if (is_master || prepare(tx, rx, len)) {
        return -1;
    }
    GPIO_BSRR(SPI_SLAVE_READY_PORT) = SPI_SLAVE_READY_MASK;
    // Idle slaves may wait forever. RXP also catches a short completed burst.
    while ((GPIO_IDR(SPI_SLAVE_NSS_PORT) & SPI_SLAVE_NSS_MASK) && !(SPI_SR & SPI_SR_RXP));
    result = receive(rx, len);
    GPIO_BSRR(SPI_SLAVE_READY_PORT) = SPI_SLAVE_READY_MASK << 16;
    if (wait_pin(SPI_SLAVE_NSS_PORT, SPI_SLAVE_NSS_MASK, 1)) {
        result = -1;
    }
    SPI_CR1 &= ~SPI_CR1_SPE;
    return result;
}
