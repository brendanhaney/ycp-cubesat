#include <config.h>
#include <registers.h>

#include "spi.h"

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

static int wait_pin(uint32_t port, uint32_t mask, int high) {
    uint32_t remaining = SPI_POLL_LIMIT;

    while (!!(GPIO_IDR(port) & mask) != high) {
        if (!--remaining) {
            return -1;
        }
    }
    return 0;
}

int spi4_init(int master) {
    uint32_t remaining = SPI_POLL_LIMIT;
    is_master = !!master;

    RCC_CR |= 1U; // HSI on; reset HSI divider is 1 (64 MHz).
    while (!(RCC_CR & (1U << 2))) {
        if (!--remaining) {
            return -1;
        }
    }
    RCC_D2CCIP1R = (RCC_D2CCIP1R & ~(7U << 16)) | (3U << 16);
    RCC_AHB4ENR |= (1U << 1) | (1U << 4); // GPIOB, GPIOE
    RCC_APB2ENR |= 1U << 13; // SPI4
    (void)RCC_APB2ENR;

    // PE2=SCK, PE5=MISO, PE6=MOSI: AF5, push-pull, medium speed.
    GPIO_MODER(GPIOE_BASE) = (GPIO_MODER(GPIOE_BASE) & ~0x3C30U) | 0x2820U;
    GPIO_AFRL(GPIOE_BASE) = (GPIO_AFRL(GPIOE_BASE) & ~0x0FF00F00U) | 0x05500500U;
    GPIO_OTYPER(GPIOE_BASE) &= ~0x64U;
    GPIO_OSPEEDR(GPIOE_BASE) = (GPIO_OSPEEDR(GPIOE_BASE) & ~0x3C30U) | 0x1410U;
    GPIO_PUPDR(GPIOE_BASE) &= ~0x3C30U;

    if (is_master) {
        // PE3=radio CS, PE4=payload CS. Set high before enabling outputs.
        GPIO_BSRR(GPIOE_BASE) = (1U << 3) | (1U << 4);
        GPIO_MODER(GPIOE_BASE) = (GPIO_MODER(GPIOE_BASE) & ~0x3C0U) | 0x140U;
        GPIO_OTYPER(GPIOE_BASE) &= ~0x18U;
        GPIO_PUPDR(GPIOE_BASE) &= ~0x3C0U;
        // PB0=radio ready, PB1=payload ready, both pulled down.
        GPIO_MODER(GPIOB_BASE) &= ~0xFU;
        GPIO_PUPDR(GPIOB_BASE) = (GPIO_PUPDR(GPIOB_BASE) & ~0xFU) | 0xAU;
    } else {
        // PE4=hardware NSS, active low with pull-up; PB0=ready output.
        GPIO_MODER(GPIOE_BASE) = (GPIO_MODER(GPIOE_BASE) & ~0x300U) | 0x200U;
        GPIO_AFRL(GPIOE_BASE) = (GPIO_AFRL(GPIOE_BASE) & ~0xF0000U) | 0x50000U;
        GPIO_PUPDR(GPIOE_BASE) = (GPIO_PUPDR(GPIOE_BASE) & ~0x300U) | 0x100U;
        GPIO_BSRR(GPIOB_BASE) = 1U << 16;
        GPIO_MODER(GPIOB_BASE) = (GPIO_MODER(GPIOB_BASE) & ~3U) | 1U;
        GPIO_OTYPER(GPIOB_BASE) &= ~1U;
        GPIO_PUPDR(GPIOB_BASE) &= ~3U;
    }

    SPI_CR1 = 0;
    // 8 bits, FIFO threshold 1 frame, HSI/256 = 250 kHz at reset clocks.
    SPI_CFG1 = 7U | (7U << 28);
    // Mode 0, MSB first, full duplex. NSS remains hardware-controlled on slaves.
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
    int result;
    uint32_t cs;

    if (!is_master || slave > SPI_PAYLOAD) {
        return -1;
    }
    cs = 1U << (slave + 3U);
    if (wait_pin(GPIOB_BASE, 1U << slave, 1) || prepare(tx, rx, len)) {
        return -1;
    }
    GPIO_BSRR(GPIOE_BASE) = cs << 16;
    SPI_CR1 |= SPI_CR1_CSTART;
    result = receive(rx, len);
    // Slave acknowledges completion before CS rises; it cannot rearm early.
    if (!result) {
        result = wait_pin(GPIOB_BASE, 1U << slave, 0);
    }
    GPIO_BSRR(GPIOE_BASE) = cs;
    SPI_CR1 &= ~SPI_CR1_SPE;
    return result;
}

int spi4_slave_transfer(const void *tx, void *rx, size_t len) {
    int result;

    if (is_master || prepare(tx, rx, len)) {
        return -1;
    }
    GPIO_BSRR(GPIOB_BASE) = 1U;
    // Idle slaves may wait forever. RXP also catches a short completed burst.
    while ((GPIO_IDR(GPIOE_BASE) & (1U << 4)) && !(SPI_SR & SPI_SR_RXP));
    result = receive(rx, len);
    GPIO_BSRR(GPIOB_BASE) = 1U << 16;
    if (wait_pin(GPIOE_BASE, 1U << 4, 1)) {
        result = -1;
    }
    SPI_CR1 &= ~SPI_CR1_SPE;
    return result;
}
