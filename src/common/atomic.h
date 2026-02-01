/*
 * atomic.h
 *
 *  Created on: Jan 3, 2026
 *      Author: MrTransistor
 */

#pragma once

#include <stm32f4xx.h>

// Set BASEPRI from a pointer
__STATIC_FORCEINLINE void __set_BASEPRI_ptr(uint32_t *val) {
    __set_BASEPRI(*val);
}

// Set PRIMASK from a pointer
__STATIC_FORCEINLINE void __set_PRIMASK_ptr(uint32_t *val) {
    __set_PRIMASK(*val);
}

#define ATOMIC_BLOCK(prio) for (uint32_t __basepri_save __attribute__((__cleanup__(__set_BASEPRI_ptr))) = __get_BASEPRI(), \
                                __once = (__set_BASEPRI_MAX((prio) << (8U - __NVIC_PRIO_BITS)), 1); __once; __once = 0)

#define ATOMIC_NOIRQ for (uint32_t __primask_save __attribute__((__cleanup__(__set_PRIMASK_ptr))) = __get_PRIMASK(), \
                         __once = (__disable_irq(), 1); __once; __once = 0)
