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

#endif