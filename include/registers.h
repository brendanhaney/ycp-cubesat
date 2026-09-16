#ifndef STM32_REGISTERS_H
#define STM32_REGISTERS_H

#include "typedefs.h"

// STM32H7 USART1 Base Address & Register Offsets
#define USART1_BASE  0x40011000U
#define USART_CR1    (*(volatile uint32_t *)(USART1_BASE + 0x00U))
#define USART_ISR    (*(volatile uint32_t *)(USART1_BASE + 0x1CU))
#define USART_TDR    (*(volatile uint32_t *)(USART1_BASE + 0x28U))

#define USART_CR1_UE  (1U << 0)  // USART Enable
#define USART_CR1_TE  (1U << 3)  // Transmitter Enable
#define USART_ISR_TXE (1U << 7)  // Transmit Data Register Empty

// SPI4 and board GPIOs (STM32H743, RM0433).
#define RCC_BASE       0x58024400U
#define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_D2CCIP1R   (*(volatile uint32_t *)(RCC_BASE + 0x50U))
#define RCC_AHB4ENR    (*(volatile uint32_t *)(RCC_BASE + 0xE0U))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0xF0U))

#define GPIOB_BASE     0x58020400U
#define GPIOE_BASE     0x58021000U
#define GPIO_MODER(b)  (*(volatile uint32_t *)((b) + 0x00U))
#define GPIO_OTYPER(b) (*(volatile uint32_t *)((b) + 0x04U))
#define GPIO_OSPEEDR(b) (*(volatile uint32_t *)((b) + 0x08U))
#define GPIO_PUPDR(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define GPIO_IDR(b)    (*(volatile uint32_t *)((b) + 0x10U))
#define GPIO_BSRR(b)   (*(volatile uint32_t *)((b) + 0x18U))
#define GPIO_AFRL(b)   (*(volatile uint32_t *)((b) + 0x20U))

#define SPI4_BASE      0x40013400U
#define SPI_CR1        (*(volatile uint32_t *)(SPI4_BASE + 0x00U))
#define SPI_CR2        (*(volatile uint32_t *)(SPI4_BASE + 0x04U))
#define SPI_CFG1       (*(volatile uint32_t *)(SPI4_BASE + 0x08U))
#define SPI_CFG2       (*(volatile uint32_t *)(SPI4_BASE + 0x0CU))
#define SPI_SR         (*(volatile uint32_t *)(SPI4_BASE + 0x14U))
#define SPI_IFCR       (*(volatile uint32_t *)(SPI4_BASE + 0x18U))
// Byte accesses prevent packing multiple frames into one FIFO access.
#define SPI_TXDR       (*(volatile uint8_t *)(SPI4_BASE + 0x20U))
#define SPI_RXDR       (*(volatile uint8_t *)(SPI4_BASE + 0x30U))

#define SPI_CR1_SPE    (1U << 0)
#define SPI_CR1_CSTART (1U << 9)
#define SPI_CR1_SSI    (1U << 12)
#define SPI_CFG2_MASTER (1U << 22)
#define SPI_CFG2_SSM   (1U << 26)
#define SPI_CFG2_AFCNTR (1U << 31)
#define SPI_SR_RXP     (1U << 0)
#define SPI_SR_TXP     (1U << 1)
#define SPI_SR_EOT     (1U << 3)
#define SPI_SR_ERRORS  ((1U << 5) | (1U << 6) | (1U << 8) | (1U << 9))

#endif
