#ifndef CUBESAT_DRIVER_GPIO_H
#define CUBESAT_DRIVER_GPIO_H

#include <registers.h>

static inline void gpio_alternate(uintptr_t port, unsigned int pin, unsigned int af, unsigned int pull) {

    unsigned int shift = pin * 2U;
    unsigned int af_shift = (pin % 8U) * 4U;

    if (pin < 8U)
        GPIO_AFRL(port) = (GPIO_AFRL(port) & ~(15U << af_shift)) | (af << af_shift);
    else
        GPIO_AFRH(port) = (GPIO_AFRH(port) & ~(15U << af_shift)) | (af << af_shift);

    GPIO_OTYPER(port) &= ~(1U << pin);
    GPIO_OSPEEDR(port) = (GPIO_OSPEEDR(port) & ~(3U << shift)) | (1U << shift);
    GPIO_PUPDR(port) = (GPIO_PUPDR(port) & ~(3U << shift)) | (pull << shift);
    GPIO_MODER(port) = (GPIO_MODER(port) & ~(3U << shift)) | (2U << shift);
}

#endif
