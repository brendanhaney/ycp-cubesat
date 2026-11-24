#include <stdint.h>
#include <config.h>
#include <registers.h>
#include <payload_spi_dma.h>
#include <image_protocol.h>

#if defined(CUBESAT_STM32H7)

/* STM32H753 RM0433 and ST's stm32h753xx.h / stm32h7xx_hal_dma.h:
 * DMA1 streams 0/1 use DMAMUX1 channels 0/1; SPI1 RX/TX requests are 37/38.
 * Reserve these two streams for this link. Other DMA streams are untouched. */
#define DMA_STATUS       MMIO32(0x40020000U)
#define DMA_CLEAR        MMIO32(0x40020008U)
#define RX_REG(offset)   MMIO32(0x40020010U + (offset))
#define TX_REG(offset)   MMIO32(0x40020028U + (offset))
#define MUX_RX           MMIO32(0x40020800U)
#define MUX_TX           MMIO32(0x40020804U)
#define MUX_STATUS       MMIO32(0x40020880U)
#define MUX_CLEAR        MMIO32(0x40020884U)
#define DMA_ENABLE       (1U << 0)
#define DMA_INCREMENT    (1U << 10)
#define DMA_MEMORY_TO_SPI (1U << 6)
#define RX_FLAGS         0x003DU
#define TX_FLAGS         0x0F40U
#define DMA_ERRORS       ((1U << 0) | (1U << 2) | (1U << 3) | \
                          (1U << 6) | (1U << 8) | (1U << 9))
#define DMA_COMPLETE     ((1U << 5) | (1U << 11))
#define AXI_FIRST        0x24000000U
#define AXI_END          0x24080000U
#define CACHE_LINE       32U
#define SCB_CCR          MMIO32(0xE000ED14U)
#define CACHE_INVALIDATE 0xE000EF5CU
#define CACHE_CLEAN      0xE000EF68U
#define CACHE_CLEAN_INVALIDATE 0xE000EF70U

enum dma_state { DMA_IDLE, DMA_ACTIVE, DMA_DONE, DMA_FAILED };
static enum dma_state state;
static int initialized;
static uintptr_t receive_address;
static uint32_t receive_length;

static void memory_barrier(void) {
#if defined(__arm__) || defined(__thumb__)
    __asm volatile ("dsb sy\nisb sy" ::: "memory");
#else
    __asm volatile ("" ::: "memory");
#endif
}

/* Buffers own complete cache lines, so maintenance cannot discard unrelated
 * dirty data. The current H7 startup leaves D-cache off; handle it if enabled. */
static void maintain_cache(uintptr_t operation, uintptr_t address, uint32_t length) {
    memory_barrier();
    if (SCB_CCR & (1U << 16)) {
        for (uint32_t offset = 0; offset < length; offset += CACHE_LINE)
            MMIO32(operation) = (uint32_t)(address + offset);
    }
    memory_barrier();
}

static int disable_streams(void) {
    RX_REG(0x00U) &= ~DMA_ENABLE;
    TX_REG(0x00U) &= ~DMA_ENABLE;
    uint32_t remaining = PAYLOAD_SPI_DMA_STOP_LIMIT;
    do {
        if (!((RX_REG(0x00U) | TX_REG(0x00U)) & DMA_ENABLE)) {
            memory_barrier();
            return 0;
        }
    } while (--remaining);
    return -1;
}

static int valid_buffer(uintptr_t address, uint32_t length) {
    return !(address & (CACHE_LINE - 1U)) && address >= AXI_FIRST &&
        address < AXI_END && length <= AXI_END - address;
}

int payload_spi_dma_init(void) {
    if (initialized) {
        payload_spi_dma_stop();
        return state == DMA_FAILED ? -1 : 0;
    }
    /* DMAMUX1 has no independent clock-enable bit on the STM32H753. */
    MMIO32(STM32H7_RCC_BASE + 0xD8U) |= 1U;
    (void)MMIO32(STM32H7_RCC_BASE + 0xD8U);
    if (disable_streams()) {
        state = DMA_FAILED;
        return -1;
    }
    MUX_RX = 37U;
    MUX_TX = 38U;
    DMA_CLEAR = RX_FLAGS | TX_FLAGS;
    MUX_CLEAR = 3U;
    receive_address = 0;
    receive_length = 0;
    state = DMA_IDLE;
    initialized = 1;
    return 0;
}

int payload_spi_dma_start(const uint8_t *tx, uint8_t *rx, uint32_t length) {
    const uintptr_t transmit = (uintptr_t)tx;
    const uintptr_t receive = (uintptr_t)rx;
    /* Fixed-size frames and separate cache-line ownership keep the contract
     * simple. Reject DTCM/flash pointers instead of silently running bad DMA. */
    if (!initialized || state != DMA_IDLE || length != IMAGE_SPI_FRAME_SIZE ||
        !valid_buffer(transmit, length) || !valid_buffer(receive, length) ||
        (transmit < receive + length && receive < transmit + length))
        return -1;
    if (disable_streams()) {
        state = DMA_FAILED;
        return -1;
    }

    DMA_CLEAR = RX_FLAGS | TX_FLAGS;
    MUX_CLEAR = 3U;
    RX_REG(0x00U) = DMA_INCREMENT | (2U << 16); // Byte P2M, high priority.
    TX_REG(0x00U) = DMA_INCREMENT | DMA_MEMORY_TO_SPI | (1U << 16);
    RX_REG(0x04U) = length;
    TX_REG(0x04U) = length;
    RX_REG(0x08U) = 0x40013030U; // SPI1 RXDR, fixed peripheral address.
    TX_REG(0x08U) = 0x40013020U; // SPI1 TXDR.
    RX_REG(0x0CU) = (uint32_t)receive;
    TX_REG(0x0CU) = (uint32_t)transmit;
    RX_REG(0x14U) = 1U << 2; // FIFO, quarter threshold, single byte transfers.
    TX_REG(0x14U) = 1U << 2;
    receive_address = receive;
    receive_length = length;
    maintain_cache(CACHE_CLEAN, transmit, length);
    maintain_cache(CACHE_CLEAN_INVALIDATE, receive, length);
    state = DMA_ACTIVE;
    /* Arm reception first. The generic driver owns SPI DMA request enables,
     * SPE/CSTART/CS and its transfer deadline; this backend never starts SCK. */
    RX_REG(0x00U) |= DMA_ENABLE;
    TX_REG(0x00U) |= DMA_ENABLE;
    memory_barrier();
    return 0;
}

int payload_spi_dma_status(void) {
    if (!initialized || state == DMA_IDLE || state == DMA_FAILED)
        return -1;
    if (state == DMA_DONE)
        return 1;
    const uint32_t status = DMA_STATUS;
    if ((status & DMA_ERRORS) || (MUX_STATUS & 3U)) {
        state = DMA_FAILED;
        return -1;
    }
    if ((status & DMA_COMPLETE) != DMA_COMPLETE)
        return 0;
    if (RX_REG(0x04U) || TX_REG(0x04U)) {
        state = DMA_FAILED;
        return -1;
    }
    maintain_cache(CACHE_INVALIDATE, receive_address, receive_length);
    state = DMA_DONE;
    /* DMA completion means RAM is ready, not that the last SCK edge has
     * finished. The generic driver must still check SPI EOT and idle SCK. */
    return 1;
}

void payload_spi_dma_stop(void) {
    if (!initialized)
        return;
    /* Caller stops SPI DMA requests before cancellation. Do not recycle RAM
     * or reconfigure a stream until hardware acknowledges clearing EN. */
    if (disable_streams()) {
        state = DMA_FAILED;
        return;
    }
    if (receive_length)
        maintain_cache(CACHE_INVALIDATE, receive_address, receive_length);
    DMA_CLEAR = RX_FLAGS | TX_FLAGS;
    MUX_CLEAR = 3U;
    receive_address = 0;
    receive_length = 0;
    state = DMA_IDLE;
}

#endif
