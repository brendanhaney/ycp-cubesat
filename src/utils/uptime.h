#ifndef CUBESAT_UPTIME_H
#define CUBESAT_UPTIME_H

#include <stdint.h>

int uptime_init(uint32_t timer_clock_hz);

uint32_t uptime_ms(void);

#endif
