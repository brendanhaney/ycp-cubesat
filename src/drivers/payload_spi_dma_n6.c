#include <config.h>
#include <payload_spi_dma.h>

#if defined(CUBESAT_STM32N6)
#include "stm32n6xx.h"

/* SPI5 requests from RM0486 / the STM32N6 HAL request table. These channels
 * are separate from the camera and SDMMC engines. GPDMA port 0 accesses the
 * peripheral bus; port 1 accesses the AXISRAM containing the link buffers. */
#define RX_CHANNEL GPDMA1_Channel0_S
#define TX_CHANNEL GPDMA1_Channel1_S
#define RX_REQUEST 87U
#define TX_REQUEST 88U
#define CHANNEL_MASK 3U
#define DMA_ERRORS (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_TOF)
#define DMA_FLAGS (DMA_CSR_TCF | DMA_CSR_HTF | DMA_ERRORS | DMA_CSR_SUSPF)
#define DMA_INTERRUPTS (DMA_CCR_TCIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE | \
                        DMA_CCR_USEIE | DMA_CCR_TOIE)
#define STOP_POLL_LIMIT 100000U

static int initialized;
static volatile int active, result;
static uint32_t completed;

static uint32_t lock_interrupts(void) {
    uint32_t previous = __get_PRIMASK();
    __disable_irq();
    return previous;
}

static void unlock_interrupts(uint32_t previous) {
    __set_PRIMASK(previous);
}

static int stop_channel(DMA_Channel_TypeDef *channel) {
    uint32_t remaining = STOP_POLL_LIMIT;
    channel->CCR &= ~DMA_INTERRUPTS;
    if (channel->CCR & DMA_CCR_EN) {
        channel->CCR |= DMA_CCR_SUSP;
        while ((channel->CCR & DMA_CCR_EN) &&
               !(channel->CSR & DMA_CSR_SUSPF)) {
            if (!--remaining)
                return -1;
        }
    }
    /* Reset only this channel, including its internal FIFO. */
    channel->CCR |= DMA_CCR_RESET;
    remaining = STOP_POLL_LIMIT;
    while (channel->CCR & (DMA_CCR_EN | DMA_CCR_RESET)) {
        if (!--remaining)
            return -1;
    }
    channel->CFCR = DMA_FLAGS;
    return 0;
}

/* Called with interrupts masked; return a notification only once per frame. */
static int update_status(void) {
    /* Stop owns the suspension flags once it clears active. A previously
     * pending IRQ must not consume SUSPF while stop_channel waits for it. */
    if (!active)
        return 0;
    const uint32_t rx_status = RX_CHANNEL->CSR;
    const uint32_t tx_status = TX_CHANNEL->CSR;
    RX_CHANNEL->CFCR = rx_status & DMA_FLAGS;
    TX_CHANNEL->CFCR = tx_status & DMA_FLAGS;
    if ((rx_status | tx_status) & DMA_ERRORS) {
        result = -1;
    } else {
        if (rx_status & DMA_CSR_TCF)
            completed |= 1U;
        if (tx_status & DMA_CSR_TCF)
            completed |= 2U;
        if (completed != CHANNEL_MASK)
            return 0;
        result = 1;
    }
    active = 0;
    RX_CHANNEL->CCR &= ~DMA_INTERRUPTS;
    TX_CHANNEL->CCR &= ~DMA_INTERRUPTS;
    /* DMA writes must be visible before the driver publishes the RX slot.
     * The aligned .bss slots are already noncacheable in the payload MPU. */
    __DMB();
    return result;
}

int payload_spi_dma_init(void) {
    initialized = 0;
    RCC_S->AHB1ENSR = RCC_AHB1ENSR_GPDMA1ENS;
    RCC_S->AHB1LPENSR = RCC_AHB1LPENSR_GPDMA1LPENS;
    (void)RCC_S->AHB1ENR;
    /* Secure firmware, secure AXISRAM and the secure SPI5 alias. Do not
     * change the attributes or clock state of any other DMA channel. */
    GPDMA1_S->SECCFGR |= CHANNEL_MASK;
    GPDMA1_S->PRIVCFGR |= CHANNEL_MASK;
    active = result = 0;
    completed = 0;
    if (stop_channel(RX_CHANNEL) || stop_channel(TX_CHANNEL))
        return -1;
    NVIC_ClearPendingIRQ(GPDMA1_Channel0_IRQn);
    NVIC_ClearPendingIRQ(GPDMA1_Channel1_IRQn);
    NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5U);
    NVIC_SetPriority(GPDMA1_Channel1_IRQn, 5U);
    NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
    initialized = 1;
    return 0;
}

int payload_spi_dma_start(const uint8_t *tx, uint8_t *rx, uint32_t length) {
    if (!initialized || !tx || !rx || !length || length > DMA_CBR1_BNDT)
        return -1;
    const uint32_t previous = lock_interrupts();
    if (active || (RX_CHANNEL->CCR & (DMA_CCR_EN | DMA_CCR_RESET)) ||
        (TX_CHANNEL->CCR & (DMA_CCR_EN | DMA_CCR_RESET))) {
        unlock_interrupts(previous);
        return -1;
    }
    /* Byte width, single-beat bursts and normal block completion events.
     * Only the RAM side increments; both bus transactions are secure. */
    RX_CHANNEL->CCR = DMA_CCR_PRIO | DMA_INTERRUPTS;
    RX_CHANNEL->CTR1 = DMA_CTR1_DINC | DMA_CTR1_DAP |
                       DMA_CTR1_SSEC | DMA_CTR1_DSEC;
    RX_CHANNEL->CTR2 = RX_REQUEST;
    RX_CHANNEL->CBR1 = length;
    RX_CHANNEL->CSAR = (uint32_t)(uintptr_t)&SPI5_S->RXDR;
    RX_CHANNEL->CDAR = (uint32_t)(uintptr_t)rx;
    RX_CHANNEL->CLLR = 0;
    RX_CHANNEL->CFCR = DMA_FLAGS;
    TX_CHANNEL->CCR = DMA_CCR_PRIO | DMA_INTERRUPTS;
    TX_CHANNEL->CTR1 = DMA_CTR1_SINC | DMA_CTR1_SAP |
                       DMA_CTR1_SSEC | DMA_CTR1_DSEC;
    TX_CHANNEL->CTR2 = TX_REQUEST | DMA_CTR2_DREQ;
    TX_CHANNEL->CBR1 = length;
    TX_CHANNEL->CSAR = (uint32_t)(uintptr_t)tx;
    TX_CHANNEL->CDAR = (uint32_t)(uintptr_t)&SPI5_S->TXDR;
    TX_CHANNEL->CLLR = 0;
    TX_CHANNEL->CFCR = DMA_FLAGS;
    NVIC_ClearPendingIRQ(GPDMA1_Channel0_IRQn);
    NVIC_ClearPendingIRQ(GPDMA1_Channel1_IRQn);
    completed = 0;
    result = 0;
    active = 1;
    __DMB();
    RX_CHANNEL->CCR |= DMA_CCR_EN;
    TX_CHANNEL->CCR |= DMA_CCR_EN;
    __DSB();
    unlock_interrupts(previous);
    return 0;
}

int payload_spi_dma_status(void) {
    if (!initialized)
        return -1;
    const uint32_t previous = lock_interrupts();
    const int notification = update_status();
    const int status = result;
    unlock_interrupts(previous);
    if (notification)
        payload_spi_dma_complete(notification < 0);
    return status;
}

void payload_spi_dma_stop(void) {
    if (!initialized)
        return;
    const uint32_t previous = lock_interrupts();
    active = result = 0;
    completed = 0;
    RX_CHANNEL->CCR &= ~DMA_INTERRUPTS;
    TX_CHANNEL->CCR &= ~DMA_INTERRUPTS;
    unlock_interrupts(previous);
    const int rx_error = stop_channel(RX_CHANNEL);
    const int tx_error = stop_channel(TX_CHANNEL);
    if (rx_error || tx_error)
        result = -1;
    NVIC_ClearPendingIRQ(GPDMA1_Channel0_IRQn);
    NVIC_ClearPendingIRQ(GPDMA1_Channel1_IRQn);
    __DSB();
}

void GPDMA1_Channel0_IRQHandler(void) {
    (void)payload_spi_dma_status();
}

void GPDMA1_Channel1_IRQHandler(void) {
    (void)payload_spi_dma_status();
}
#endif
