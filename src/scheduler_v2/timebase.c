/*
 * timebase.c
 *
 *  Created on: Jan 9, 2026
 *      Author: MrTransistor
 */

#include "common/atomic.h"

#include "timebase.h"

static volatile osTimestamp_t _timestamp;

void SysTick_Handler(void) {
    // Since we are in the interrupt handler, there is no need to save PRIMASK
    // as it is definitely reset
    __disable_irq();
    _timestamp += US_PER_SYS_TICK;
    (void) (SysTick->CTRL); // clear COUNTFLAG

    // Trigger a context switch
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __enable_irq();
}

void timebase_init(void) {
    SysTick_Config(SYS_TICK_nCYCLES);
}

osTimestamp_t osTime(void) {
    uint64_t _time;
    ATOMIC_NOIRQ
    {
        // 64 bit reads are not atomic, prevent value corruption by SysTick interrupt
        _time = _timestamp;
    }
    return _time;
}
