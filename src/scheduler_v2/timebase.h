/*
 * timebase.h
 *
 *  Created on: Jan 9, 2026
 *      Author: MrTransistor
 */

#pragma once

#include <stdint.h>

// CPU clock frequency
#define CORE_FREQ_HZ        168000000UL
// Kernel tick frequency
#define SYS_TICK_FREQ_HZ    1000UL

// Core clock ticks per kernel tick
#define SYS_TICK_nCYCLES    (CORE_FREQ_HZ / SYS_TICK_FREQ_HZ)
// Microseconds per kernel tick
#define US_PER_SYS_TICK     (1000000UL / SYS_TICK_FREQ_HZ)

#define TIME_A_BEFORE_B(a, b) ((int64_t)(a - b) < 0)
#define TIME_A_AFTER_B(a, b) TIME_A_BEFORE_B(b, a)

typedef uint64_t osTimestamp_t;

// Time in microseconds since system startup
// Updates in steps of US_PER_SYS_TICK
osTimestamp_t osTime(void);

// Inits system timebase
void timebase_init(void);
