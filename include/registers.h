#ifndef STM32_REGISTERS_H
#define STM32_REGISTERS_H

#include "typedefs.h"

// Console: USART1 in Renode, USART3 via ST-LINK on NUCLEO-H753ZI.
#define USART1_BASE  0x40011000U
#define USART3_BASE  0x40004800U
#ifdef CUBESAT_NUCLEO_H753ZI
#define CONSOLE_USART_BASE USART3_BASE
#else
#define CONSOLE_USART_BASE USART1_BASE
#endif
#define USART_CR1    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x00U))
#define USART_CR2    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x04U))
#define USART_CR3    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x08U))
#define USART_BRR    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x0CU))
#define USART_ISR    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x1CU))
#define USART_TDR    (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x28U))
#define USART_PRESC  (*(volatile uint32_t *)(CONSOLE_USART_BASE + 0x2CU))

#define USART_CR1_UE  (1U << 0)  // USART Enable
#define USART_CR1_TE  (1U << 3)  // Transmitter Enable
#define USART_ISR_TXE (1U << 7)  // Transmit Data Register Empty

// SPI4 and board GPIOs (STM32H743, RM0433).
#define RCC_BASE       0x58024400U
#define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_D2CCIP1R   (*(volatile uint32_t *)(RCC_BASE + 0x50U))
#define RCC_D2CCIP2R   (*(volatile uint32_t *)(RCC_BASE + 0x54U))
#define RCC_APB1LRSTR  (*(volatile uint32_t *)(RCC_BASE + 0x90U))
#define RCC_AHB4ENR    (*(volatile uint32_t *)(RCC_BASE + 0xE0U))
#define RCC_APB1LENR   (*(volatile uint32_t *)(RCC_BASE + 0xE8U))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0xF0U))

#define GPIOB_BASE     0x58020400U
#define GPIOD_BASE     0x58020C00U
#define GPIOE_BASE     0x58021000U
#define GPIOF_BASE     0x58021400U
#define GPIO_MODER(b)  (*(volatile uint32_t *)((b) + 0x00U))
#define GPIO_OTYPER(b) (*(volatile uint32_t *)((b) + 0x04U))
#define GPIO_OSPEEDR(b) (*(volatile uint32_t *)((b) + 0x08U))
#define GPIO_PUPDR(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define GPIO_IDR(b)    (*(volatile uint32_t *)((b) + 0x10U))
#define GPIO_BSRR(b)   (*(volatile uint32_t *)((b) + 0x18U))
#define GPIO_AFRL(b)   (*(volatile uint32_t *)((b) + 0x20U))
#define GPIO_AFRH(b)   (*(volatile uint32_t *)((b) + 0x24U))

// STM32H753 I2C2 (RM0433).
#define I2C2_BASE      0x40005800U
#define I2C_CR1        (*(volatile uint32_t *)(I2C2_BASE + 0x00U))
#define I2C_CR2        (*(volatile uint32_t *)(I2C2_BASE + 0x04U))
#define I2C_TIMINGR    (*(volatile uint32_t *)(I2C2_BASE + 0x10U))
#define I2C_ISR        (*(volatile uint32_t *)(I2C2_BASE + 0x18U))
#define I2C_ICR        (*(volatile uint32_t *)(I2C2_BASE + 0x1CU))
#define I2C_RXDR       (*(volatile uint32_t *)(I2C2_BASE + 0x24U))
#define I2C_TXDR       (*(volatile uint32_t *)(I2C2_BASE + 0x28U))

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
