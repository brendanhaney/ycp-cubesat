#include <stdint.h>
#include <string.h>
#include <config.h>
#include <registers.h>
#include <payload_spi.h>

#ifndef PAYLOAD_SPI_POLL_LIMIT
#define PAYLOAD_SPI_POLL_LIMIT 1000000U
#endif

#if defined(CUBESAT_STM32H7)
#define LINK_SPI_BASE 0x40013000U /* SPI1 */
#define LINK_GPIOA    0x58020000U
#define LINK_GPIOB    0x58020400U
#define LINK_GPIOD    0x58020C00U
#define LINK_CS_PORT  LINK_GPIOD
#define LINK_CS_MASK  (1U << 14)
#else
#define LINK_SPI_BASE 0x52005000U /* SPI5, secure alias */
#define LINK_GPIOA    0x56020000U
#define LINK_GPIOE    0x56021000U
#define LINK_GPIOG    0x56021800U
#define LINK_CS_PORT  LINK_GPIOA
#define LINK_CS_MASK  (1U << 3)
/* STM32N657xx CMSIS: SPI5_IRQn=157, __NVIC_PRIO_BITS=4. Keep CMSIS out
 * of this raw-register driver to avoid conflicting register definitions. */
#define LINK_IRQ      157U
#define LINK_IRQ_BIT  (1U << (LINK_IRQ % 32U))
#define LINK_IRQ_WORD ((LINK_IRQ / 32U) * 4U)
#define LINK_IRQ_ENABLE MMIO32(0xE000E100U + LINK_IRQ_WORD)
#define LINK_IRQ_CLEAR  MMIO32(0xE000E280U + LINK_IRQ_WORD)
#endif

#define LINK_REG(offset) MMIO32(LINK_SPI_BASE + (offset))
#define LINK_TX MMIO8(LINK_SPI_BASE + 0x20U)
#define LINK_RX MMIO8(LINK_SPI_BASE + 0x30U)

static int initialized;
#if defined(CUBESAT_STM32N6)
static volatile int armed, failed;
static volatile unsigned int received, transmitted;
static uint8_t response[PAYLOAD_SPI_SIZE];
static uint8_t request[PAYLOAD_SPI_SIZE];
#endif

static void alternate(uintptr_t port, unsigned int pin, unsigned int pull) {
    const unsigned int shift = pin * 2U;
    const unsigned int af_shift = (pin % 8U) * 4U;
    if (pin < 8U)
        GPIO_AFRL(port) = (GPIO_AFRL(port) & ~(15U << af_shift)) | (5U << af_shift);
    else
        GPIO_AFRH(port) = (GPIO_AFRH(port) & ~(15U << af_shift)) | (5U << af_shift);
    GPIO_OTYPER(port) &= ~(1U << pin);
    GPIO_OSPEEDR(port) = (GPIO_OSPEEDR(port) & ~(3U << shift)) | (1U << shift);
    GPIO_PUPDR(port) = (GPIO_PUPDR(port) & ~(3U << shift)) | (pull << shift);
    GPIO_MODER(port) = (GPIO_MODER(port) & ~(3U << shift)) | (2U << shift);
}

static void reset_spi(void) {
#if defined(CUBESAT_STM32H7)
    MMIO32(STM32H7_RCC_BASE + 0x98U) |= 1U << 12;
    MMIO32(STM32H7_RCC_BASE + 0x98U) &= ~(1U << 12);
#else
    STM32N6_RCC_APB2RSTSR = 1U << 20;
    STM32N6_RCC_APB2RSTCR = 1U << 20;
#endif
}

#if defined(CUBESAT_STM32H7)
static int wait_status(uint32_t flag) {
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    do {
        const uint32_t status = LINK_REG(0x14U);
        if (status & SPI_SR_ERRORS)
            return -1;
        if (status & flag)
            return 0;
    } while (--remaining);
    return -1;
}
#endif

static int prepare(const uint8_t *tx) {
    reset_spi(); // Flush partial frames and errors from an interrupted request.
#if defined(CUBESAT_STM32H7)
    // Internal NSS must be inactive before entering master mode, otherwise
    // mode-fault detection can clear MASTER before the transfer starts.
    LINK_REG(0x00U) = SPI_CR1_SSI;
#else
    LINK_REG(0x00U) = 0;
#endif
    // Change only the master's SPI divider, not CPU or peripheral clocks.
#if defined(CUBESAT_STM32H7)
    const uint32_t prescaler = PAYLOAD_SPI_MASTER_MBR;
#else
    const uint32_t prescaler = 5U; // Ignored in slave mode; retain its setup.
#endif
    // Eight-bit DSIZE/CRCSIZE; H753 ES0392 2.22.5 applies even without CRC.
    LINK_REG(0x08U) = 7U | (7U << 16) | (prescaler << 28);
#if defined(CUBESAT_STM32H7)
    LINK_REG(0x0CU) = SPI_CFG2_AFCNTR | SPI_CFG2_MASTER | SPI_CFG2_SSM;
    LINK_REG(0x04U) = PAYLOAD_SPI_SIZE;
#else
    // Physical CS is polled; software NSS selects the slave peripheral.
    LINK_REG(0x0CU) = SPI_CFG2_SSM;
    LINK_REG(0x04U) = 0; // IRQ counts bytes; no EOT or suspend between bytes.
#endif
    LINK_REG(0x18U) = 0xFF8U;
    LINK_REG(0x00U) |= SPI_CR1_SPE;
    // Eight bytes fit both peripherals. Never wait while arming the slave.
    for (unsigned int i = 0; i < 8U; i++) {
        const uint32_t status = LINK_REG(0x14U);
        if ((status & SPI_SR_ERRORS) || !(status & SPI_SR_TXP))
            return -1;
        LINK_TX = tx[i];
    }
    return 0;
}

int payload_spi_init(void) {
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    initialized = 0;
#if defined(CUBESAT_STM32N6)
    armed = failed = 0;
    received = transmitted = 0;
    memset(response, 0, sizeof(response));
#endif
#if defined(CUBESAT_STM32H7)
    STM32H7_RCC_CR |= 1U;
    while (!(STM32H7_RCC_CR & (1U << 2))) {
        if (!--remaining)
            return -1;
    }
    // SPI123SEL = per_ck; CKPERSEL = HSI. Do not change the CPU clock.
    MMIO32(STM32H7_RCC_BASE + 0x4CU) &= ~(3U << 28);
    STM32H7_RCC_D2CCIP1R = (STM32H7_RCC_D2CCIP1R & ~(7U << 12)) | (4U << 12);
    STM32H7_RCC_AHB4ENR |= (1U << 0) | (1U << 1) | (1U << 3);
    STM32H7_RCC_APB2ENR |= 1U << 12;
    (void)STM32H7_RCC_AHB4ENR;
    (void)STM32H7_RCC_APB2ENR;
    GPIO_BSRR(LINK_GPIOD) = LINK_CS_MASK;
    GPIO_OTYPER(LINK_GPIOD) &= ~LINK_CS_MASK;
    GPIO_PUPDR(LINK_GPIOD) &= ~(3U << 28);
    GPIO_OSPEEDR(LINK_GPIOD) = (GPIO_OSPEEDR(LINK_GPIOD) & ~(3U << 28)) | (1U << 28);
    GPIO_MODER(LINK_GPIOD) = (GPIO_MODER(LINK_GPIOD) & ~(3U << 28)) | (1U << 28);
    alternate(LINK_GPIOA, 5U, 0U); // D13 SCK
    alternate(LINK_GPIOA, 6U, 2U); // D12 MISO; absent/busy slave reads zero.
    alternate(LINK_GPIOB, 5U, 0U); // D11 MOSI
#else
    STM32N6_RCC_CR |= 1U << 3;
    while (!(STM32N6_RCC_SR & (1U << 3))) {
        if (!--remaining)
            return -1;
    }
    STM32N6_RCC_BUSENSR = (1U << 6) | (1U << 9);
    (void)STM32N6_RCC_BUSENR;
    STM32N6_RCC_AHB4ENSR = (1U << 18) | (1U << 0) | (1U << 4) | (1U << 6);
    (void)STM32N6_RCC_AHB4ENR;
    STM32N6_PWR_SVMCR3 &= ~(1U << 24);
    STM32N6_RCC_CCIPR9 &= ~(7U << 20); // SPI5 kernel clock = PCLK2.
    STM32N6_RCC_APB2ENSR = 1U << 20;
    (void)STM32N6_RCC_APB2ENR;
    GPIO_MODER(LINK_GPIOA) &= ~(3U << 6); // D10: GPIO input, software NSS.
    GPIO_PUPDR(LINK_GPIOA) = (GPIO_PUPDR(LINK_GPIOA) & ~(3U << 6)) | (1U << 6);
    alternate(LINK_GPIOG, 2U, 0U);  // D11 MOSI
    alternate(LINK_GPIOG, 1U, 0U);  // D12 MISO
    alternate(LINK_GPIOE, 15U, 0U); // D13 SCK
    LINK_IRQ_CLEAR = LINK_IRQ_BIT;
    MMIO8(0xE000E400U + LINK_IRQ) = 5U << 4; // Above camera IRQ priority 6.
    LINK_IRQ_ENABLE = LINK_IRQ_BIT;
#endif
    reset_spi();
    LINK_REG(0x00U) = 0;
    LINK_REG(0x10U) = 0;
    initialized = 1;
    return 0;
}

int payload_spi_exchange(const uint8_t tx[PAYLOAD_SPI_SIZE],
                         uint8_t rx[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32H7)
    int result = -1;
    if (!initialized || !tx || !rx)
        return -1;
    if (prepare(tx))
        goto done;
    unsigned int sent = 8U, received_bytes = 0;
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    GPIO_BSRR(LINK_CS_PORT) = LINK_CS_MASK << 16;
    LINK_REG(0x00U) |= SPI_CR1_CSTART;
    while (received_bytes < PAYLOAD_SPI_SIZE) {
        const uint32_t status = LINK_REG(0x14U);
        if (status & SPI_SR_ERRORS)
            goto done;
        int progress = 0;
        // Drain and refill together so neither FIFO waits for the whole frame.
        if (status & SPI_SR_RXP) {
            rx[received_bytes++] = LINK_RX;
            progress = 1;
        }
        if ((status & SPI_SR_TXP) && sent < PAYLOAD_SPI_SIZE) {
            LINK_TX = tx[sent++];
            progress = 1;
        }
        if (progress)
            remaining = PAYLOAD_SPI_POLL_LIMIT;
        else if (!--remaining)
            goto done;
    }
    result = wait_status(SPI_SR_EOT);
    if (!result) {
        // EOT can precede the final falling SCK edge (ES0392 2.22.6).
        remaining = PAYLOAD_SPI_POLL_LIMIT;
        while (GPIO_IDR(LINK_GPIOA) & (1U << 5)) {
            if (!--remaining) {
                result = -1;
                break;
            }
        }
    }
done:
    GPIO_BSRR(LINK_CS_PORT) = LINK_CS_MASK;
    LINK_REG(0x00U) &= ~SPI_CR1_SPE;
    return result;
#else
    (void)tx;
    (void)rx;
    return -1;
#endif
}

void payload_spi_pause(void) {
#if defined(CUBESAT_STM32N6)
    if (initialized) {
        LINK_REG(0x10U) = 0; // Stop the source before clearing a pending IRQ.
        LINK_IRQ_CLEAR = LINK_IRQ_BIT;
        LINK_REG(0x00U) &= ~SPI_CR1_SPE;
    }
    armed = failed = 0;
    received = transmitted = 0;
#endif
}

void payload_spi_set_response(const uint8_t next[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32N6)
    if (!armed && next)
        memcpy(response, next, sizeof(response));
#else
    (void)next;
#endif
}

#if defined(CUBESAT_STM32N6)
void SPI5_IRQHandler(void) {
    if (!armed)
        return;
    // Each iteration consumes available work. This never waits for clocks.
    for (;;) {
        const uint32_t status = LINK_REG(0x14U);
        if (status & SPI_SR_ERRORS) {
            failed = 1;
            LINK_REG(0x10U) = 0;
            LINK_REG(0x00U) &= ~SPI_CR1_SPE;
            return;
        }
        int progress = 0;
        if (status & SPI_SR_RXP) {
            const uint8_t value = LINK_RX;
            if (received >= PAYLOAD_SPI_SIZE) {
                failed = 1;
                LINK_REG(0x10U) = 0;
                LINK_REG(0x00U) &= ~SPI_CR1_SPE;
                return;
            }
            request[received++] = value;
            progress = 1;
        }
        if ((status & SPI_SR_TXP) && transmitted < PAYLOAD_SPI_SIZE) {
            LINK_TX = response[transmitted++];
            progress = 1;
        }
        if (transmitted == PAYLOAD_SPI_SIZE)
            LINK_REG(0x10U) &= ~SPI_SR_TXP; // TXPIE is also bit 1.
        if (!progress)
            return;
    }
}
#endif

int payload_spi_service(uint8_t out[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32N6)
    if (!initialized || !out)
        return -1;
    if (!armed) {
        // Skip an in-progress frame, then preload before the next selection.
        if (!(GPIO_IDR(LINK_CS_PORT) & LINK_CS_MASK))
            return 0;
        if (prepare(response)) {
            payload_spi_pause();
            return -1;
        }
        received = 0;
        transmitted = 8U;
        failed = 0;
        armed = 1;
        LINK_REG(0x10U) = SPI_SR_RXP | SPI_SR_TXP | SPI_SR_ERRORS;
        return 0;
    }
    if (failed) {
        payload_spi_pause();
        return -1;
    }
    if (!(GPIO_IDR(LINK_CS_PORT) & LINK_CS_MASK))
        return 0;
    // No clocks remain after CS rises. Quiesce IRQ delivery before draining
    // any last FIFO bytes and reading the completed frame in the main loop.
    if (!received && !(LINK_REG(0x14U) & SPI_SR_RXP))
        return 0;
    LINK_REG(0x10U) = 0;
    LINK_IRQ_CLEAR = LINK_IRQ_BIT;
    SPI5_IRQHandler();
    const int valid = !failed && received == PAYLOAD_SPI_SIZE;
    if (valid)
        memcpy(out, request, PAYLOAD_SPI_SIZE);
    payload_spi_pause();
    return valid ? 1 : -1;
#else
    (void)out;
    return -1;
#endif
}
