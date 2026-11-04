// https://raw.githubusercontent.com/ARM-software/CMSIS_6/main/CMSIS/Core/Include/core_cm7.h
// https://arm-software.github.io/CMSIS_6/latest/Core/group__SysTick__gr.html

#ifndef STM32_DELAY_H
#define STM32_DELAY_H

#include <stdint.h>

int delay_init(uint32_t freq);

void delay_ms(uint32_t ms);

#endif
