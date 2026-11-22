#include <stdint.h>
#include <string.h>
#include <config.h>
#include <registers.h>
#include <payload_spi.h>
#include <payload_spi_dma.h>

#ifndef PAYLOAD_SPI_POLL_LIMIT
#define PAYLOAD_SPI_POLL_LIMIT 1000000U
#endif

#if defined(CUBESAT_STM32H7)
#define LINK_SPI_BASE 0x40013000U
#define LINK_GPIOA 0x58020000U
#define LINK_GPIOB 0x58020400U
#define LINK_GPIOD 0x58020C00U
#define LINK_CS_PORT LINK_GPIOD
#define LINK_CS_MASK (1U << 14)
#define LINK_READY_PORT LINK_GPIOD
#define LINK_READY_MASK (1U << 15) /* Arduino D9 */
#else
#define LINK_SPI_BASE 0x52005000U
#define LINK_GPIOA 0x56020000U
#define LINK_GPIOD 0x56020C00U
#define LINK_GPIOE 0x56021000U
#define LINK_GPIOG 0x56021800U
#define LINK_CS_PORT LINK_GPIOA
#define LINK_CS_MASK (1U << 3)
#define LINK_READY_PORT LINK_GPIOD
#define LINK_READY_MASK (1U << 7) /* Arduino D9, GPIO_EXTI7 */
#endif

#define LINK_REG(offset) MMIO32(LINK_SPI_BASE + (offset))
#define LINK_DMA_ENABLE ((1U << 14) | (1U << 15))

payload_spi_rx_buffer_t payload_spi_rx_buffer;
static uint8_t transmit[PAYLOAD_SPI_SIZE] __attribute__((aligned(32)));
static int initialized;
static volatile int armed;
#if defined(CUBESAT_STM32N6)
static int selected;
static uint32_t selected_at;
extern uint32_t HAL_GetTick(void);
#endif

static void barrier(void) {
#if defined(__arm__) || defined(__thumb__)
    __asm volatile ("dmb sy" ::: "memory");
#else
    __asm volatile ("" ::: "memory");
#endif
}

int payload_spi_receive_acquire(const uint8_t **frame) {
#if defined(CUBESAT_STM32N6)
    if (armed)
        return 0;
#endif
    if (!frame || payload_spi_rx_buffer.status != PAYLOAD_SPI_RX_READY)
        return 0;
    barrier();
    payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_PROCESSING;
    *frame = payload_spi_rx_buffer.data;
    return 1;
}

void payload_spi_receive_release(void) {
    if (payload_spi_rx_buffer.status == PAYLOAD_SPI_RX_PROCESSING) {
        barrier();
        payload_spi_rx_buffer.status = 0;
    }
}

int payload_spi_busy(void) {
#if defined(CUBESAT_STM32N6)
    return armed && !(GPIO_IDR(LINK_CS_PORT) & LINK_CS_MASK);
#else
    return 0;
#endif
}

static void copy_received(uint8_t *out) {
    const uint8_t *frame;
    if (out && payload_spi_receive_acquire(&frame)) {
        memcpy(out, frame, PAYLOAD_SPI_SIZE);
        payload_spi_receive_release();
    }
}

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

static void ready(int high) {
#if defined(CUBESAT_STM32N6)
    GPIO_BSRR(LINK_READY_PORT) = high ? LINK_READY_MASK : LINK_READY_MASK << 16;
#else
    (void)high;
#endif
}

static void stop_link(void) {
    ready(0);
    LINK_REG(0x08U) &= ~LINK_DMA_ENABLE;
    LINK_REG(0x00U) &= ~SPI_CR1_SPE;
    payload_spi_dma_stop();
    armed = 0;
}

void payload_spi_pause(void) {
    if (initialized) {
        stop_link();
        if (payload_spi_rx_buffer.status == PAYLOAD_SPI_RX_RECEIVING)
            payload_spi_rx_buffer.status = 0;
    }
}

void payload_spi_dma_complete(int error) {
    if (!armed || (payload_spi_rx_buffer.status != PAYLOAD_SPI_RX_RECEIVING &&
                   !(error && payload_spi_rx_buffer.status == PAYLOAD_SPI_RX_READY)))
        return;
    ready(0);
    barrier();
    if (error) {
        payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_ERROR;
    } else {
        payload_spi_rx_buffer.sequence++;
        payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_READY;
    }
}

static int prepare(void) {
    if (payload_spi_rx_buffer.status &
        (PAYLOAD_SPI_RX_RECEIVING | PAYLOAD_SPI_RX_READY | PAYLOAD_SPI_RX_PROCESSING))
        return -1;
    stop_link();
#if defined(CUBESAT_STM32H7)
    MMIO32(STM32H7_RCC_BASE + 0x98U) |= 1U << 12;
    MMIO32(STM32H7_RCC_BASE + 0x98U) &= ~(1U << 12);
    const uint32_t prescaler = PAYLOAD_SPI_MASTER_MBR;
#else
    STM32N6_RCC_APB2RSTSR = 1U << 20;
    STM32N6_RCC_APB2RSTCR = 1U << 20;
    LINK_REG(0x00U) = 0;
    const uint32_t prescaler = 5U;
    LINK_REG(0x0CU) = SPI_CFG2_SSM;
    selected = 0;
    selected_at = HAL_GetTick();
#endif
    LINK_REG(0x08U) = 7U | (7U << 16) | (prescaler << 28);
#if defined(CUBESAT_STM32H7)
    /* Set internal NSS before master mode. Keep AF outputs released until
     * MASTER is set, so an armed slave sees no stray clock transitions. */
    LINK_REG(0x0CU) = SPI_CFG2_SSM;
    LINK_REG(0x00U) = SPI_CR1_SSI;
    LINK_REG(0x0CU) = SPI_CFG2_AFCNTR | SPI_CFG2_MASTER | SPI_CFG2_SSM;
#endif
    LINK_REG(0x04U) = PAYLOAD_SPI_SIZE;
    LINK_REG(0x10U) = 0;
    LINK_REG(0x18U) = 0xFF8U;
    payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_RECEIVING;
    armed = 1;
    if (payload_spi_dma_start(transmit, payload_spi_rx_buffer.data, PAYLOAD_SPI_SIZE))
        goto failed;
    LINK_REG(0x08U) |= LINK_DMA_ENABLE;
#if defined(CUBESAT_STM32H7)
    LINK_REG(0x00U) = SPI_CR1_SSI | SPI_CR1_SPE;
#else
    LINK_REG(0x00U) |= SPI_CR1_SPE;
#endif
    /* Let DMA put the first bytes in TX FIFO before exposing READY/SCK. */
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    while (LINK_REG(0x14U) & SPI_SR_TXP) {
        if (payload_spi_dma_status() < 0 || !--remaining)
            goto failed;
    }
    if (payload_spi_rx_buffer.status != PAYLOAD_SPI_RX_RECEIVING ||
        payload_spi_dma_status() < 0)
        goto failed;
    return 0;
failed:
    stop_link();
    payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_ERROR;
    return -1;
}

int payload_spi_init(void) {
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    initialized = armed = 0;
    memset(&payload_spi_rx_buffer, 0, sizeof(payload_spi_rx_buffer));
    memset(transmit, 0, sizeof(transmit));
#if defined(CUBESAT_STM32H7)
    STM32H7_RCC_CR |= 1U;
    while (!(STM32H7_RCC_CR & (1U << 2)))
        if (!--remaining) return -1;
    MMIO32(STM32H7_RCC_BASE + 0x4CU) &= ~(3U << 28);
    STM32H7_RCC_D2CCIP1R = (STM32H7_RCC_D2CCIP1R & ~(7U << 12)) | (4U << 12);
    STM32H7_RCC_AHB4ENR |= (1U << 0) | (1U << 1) | (1U << 3);
    STM32H7_RCC_APB2ENR |= 1U << 12;
    (void)STM32H7_RCC_APB2ENR;
    GPIO_BSRR(LINK_GPIOD) = LINK_CS_MASK;
    GPIO_OTYPER(LINK_GPIOD) &= ~LINK_CS_MASK;
    GPIO_MODER(LINK_GPIOD) = (GPIO_MODER(LINK_GPIOD) & ~(15U << 28)) | (1U << 28);
    GPIO_PUPDR(LINK_GPIOD) &= ~(3U << 28);
    GPIO_PUPDR(LINK_GPIOD) = (GPIO_PUPDR(LINK_GPIOD) & ~(3U << 30)) | (2U << 30);
    alternate(LINK_GPIOA, 5U, 2U); // Hold SCK low during peripheral reset.
    alternate(LINK_GPIOA, 6U, 2U);
    alternate(LINK_GPIOB, 5U, 0U);
#else
    STM32N6_RCC_CR |= 1U << 3;
    while (!(STM32N6_RCC_SR & (1U << 3)))
        if (!--remaining) return -1;
    STM32N6_RCC_BUSENSR = (1U << 6) | (1U << 9);
    STM32N6_RCC_AHB4ENSR = (1U << 18) | (1U << 0) | (1U << 3) | (1U << 4) | (1U << 6);
    (void)STM32N6_RCC_AHB4ENR;
    STM32N6_PWR_SVMCR3 &= ~(1U << 24);
    STM32N6_RCC_CCIPR9 &= ~(7U << 20);
    STM32N6_RCC_APB2ENSR = 1U << 20;
    (void)STM32N6_RCC_APB2ENR;
    GPIO_MODER(LINK_GPIOA) &= ~(3U << 6);
    GPIO_PUPDR(LINK_GPIOA) = (GPIO_PUPDR(LINK_GPIOA) & ~(3U << 6)) | (1U << 6);
    ready(0);
    GPIO_MODER(LINK_GPIOD) = (GPIO_MODER(LINK_GPIOD) & ~(3U << 14)) | (1U << 14);
    GPIO_OTYPER(LINK_GPIOD) &= ~LINK_READY_MASK;
    GPIO_PUPDR(LINK_GPIOD) &= ~(3U << 14);
    alternate(LINK_GPIOG, 2U, 0U);
    alternate(LINK_GPIOG, 1U, 0U);
    alternate(LINK_GPIOE, 15U, 0U);
#endif
    if (payload_spi_dma_init())
        return -1;
    initialized = 1;
    return 0;
}

int payload_spi_exchange(const uint8_t tx[PAYLOAD_SPI_SIZE], uint8_t rx[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32H7)
    if (!initialized || !tx)
        return -1;
    uint32_t remaining = PAYLOAD_SPI_POLL_LIMIT;
    while (!(GPIO_IDR(LINK_READY_PORT) & LINK_READY_MASK))
        if (!--remaining) return -1;
    memcpy(transmit, tx, PAYLOAD_SPI_SIZE);
    if (prepare())
        return -1;
    /* Capture may withdraw the offer while the master's DMA is arming. */
    if (!(GPIO_IDR(LINK_READY_PORT) & LINK_READY_MASK)) {
        stop_link();
        payload_spi_rx_buffer.status = 0;
        return -1;
    }
    int result = -1;
    GPIO_BSRR(LINK_CS_PORT) = LINK_CS_MASK << 16;
    LINK_REG(0x00U) |= SPI_CR1_CSTART;
    remaining = PAYLOAD_SPI_POLL_LIMIT;
    int status;
    do {
        status = payload_spi_dma_status();
        if ((LINK_REG(0x14U) & SPI_SR_ERRORS) || !--remaining)
            goto done;
    } while (!status);
    if (status < 0)
        goto done;
    remaining = PAYLOAD_SPI_POLL_LIMIT;
    while (!(LINK_REG(0x14U) & SPI_SR_EOT) ||
           (GPIO_IDR(LINK_GPIOA) & (1U << 5)) ||
           (GPIO_IDR(LINK_READY_PORT) & LINK_READY_MASK))
        if ((LINK_REG(0x14U) & SPI_SR_ERRORS) || !--remaining) goto done;
    payload_spi_dma_complete(0);
    result = 0;
done:
    GPIO_BSRR(LINK_CS_PORT) = LINK_CS_MASK;
    stop_link();
    if (result)
        payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_ERROR;
    else
        copy_received(rx);
    return result;
#else
    (void)tx; (void)rx;
    return -1;
#endif
}

void payload_spi_set_response(const uint8_t next[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32N6)
    if (!armed && next)
        memcpy(transmit, next, PAYLOAD_SPI_SIZE);
#else
    (void)next;
#endif
}

int payload_spi_service(uint8_t out[PAYLOAD_SPI_SIZE]) {
#if defined(CUBESAT_STM32N6)
    if (!initialized)
        return -1;
    if (!armed) {
        if (payload_spi_rx_buffer.status == PAYLOAD_SPI_RX_READY) {
            copy_received(out);
            return 1;
        }
        if (!(GPIO_IDR(LINK_CS_PORT) & LINK_CS_MASK) ||
            payload_spi_rx_buffer.status == PAYLOAD_SPI_RX_PROCESSING)
            return 0;
        if (prepare())
            return -1;
        ready(1);
        return 0;
    }
    const int status = payload_spi_dma_status();
    if (LINK_REG(0x14U) & SPI_SR_ERRORS)
        payload_spi_dma_complete(1);
    if (!(GPIO_IDR(LINK_CS_PORT) & LINK_CS_MASK)) {
        if (!selected)
            selected_at = HAL_GetTick();
        selected = 1;
        if ((uint32_t)(HAL_GetTick() - selected_at) >= 3000U) {
            stop_link();
            payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_ERROR;
            return -1;
        }
        return 0;
    }
    if (!status && !selected && payload_spi_rx_buffer.status != PAYLOAD_SPI_RX_ERROR)
        return 0;
    stop_link();
    if (status != 1 || payload_spi_rx_buffer.status != PAYLOAD_SPI_RX_READY) {
        payload_spi_rx_buffer.status = PAYLOAD_SPI_RX_ERROR;
        return -1;
    }
    copy_received(out);
    return 1;
#else
    (void)out;
    return -1;
#endif
}
