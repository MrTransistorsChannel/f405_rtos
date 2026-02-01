/*
 * scheduler.h
 *
 *  Created on: Jan 8, 2026
 *      Author: MrTransistor
 */

#pragma once

#include <stm32f4xx.h>
#include <stdbool.h>

#include "timebase.h"

// Exceptions of priority level 15 (SysTick, PendSV) won't interrupt syscalls
#define MAX_SYSCALL_INTERRUPT_PRIORITY  (0xE << __NVIC_PRIO_BITS)
// This is set to one below the realtime priority to allow realtime tasks to
// allways take priority
#define TASK_DYN_PRIO_LIMIT     254
// Stack size for OS Idle task in words. Must be divisible by 8 to adhere to ARM EABI
#define OS_IDLE_TASK_STACK_SIZE 128

// Share CPU time between tasks automatically. Switches active task each system tick
#define OS_USE_TIME_SLICING     1

typedef enum {
    TASK_PRIORITY_IDLE = 0,     // Dynamic scheduling does not apply, only runs if no other tasks are ready
    TASK_PRIORITY_LOW = 1,
    TASK_PRIORITY_MEDIUM = 2,
    TASK_PRIORITY_HIGH = 3,
    TASK_PRIORITY_REALTIME = 4,  // Realtime tasks take absolute priority
    TASK_PRIORITY_LVL_NUM
} TaskPriority_e;

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_DELAYED,
    TASK_STATE_SUSPENDED
} TaskState_e;

// Macro definitions for common period conversions
#define TASK_FREQ_HZ(hz) (1000000 / (hz))
#define TASK_PERIOD_MS(ms) ((ms) * 1000)
#define TASK_PERIOD_US(us) (us)

typedef struct task_s {
    // Task stack
    uint32_t *stackPointer; // Current value of SP saved on context switch
    uint32_t *stackTop;     // Beginning of the memory area allocated for the stack
    uint32_t *stackBase;    // Task's stack bottom

    // List linkage pointers
    struct task_s *prev;
    struct task_s *next;

    // Timing
    osTimestamp_t lastExecTimeUs;
    osTimestamp_t nextExecTimeUs;    // Updated by delay function

    TaskState_e state;
    uint8_t staticPriority;
} Task_t;

// Used for unordered lists (delayed, suspended)
typedef struct {
    Task_t *head;
    Task_t *tail;
} TaskList_t;

// Circular doubly linked list with a priority filed
typedef struct {
    Task_t *iter;
    uint8_t dynamicPriority;
} TaskReadyQueue_t;

typedef void (*TaskFunc_t)(void);

// Initialises task's stack frame for context switching
void osInitTask(Task_t *task, TaskFunc_t taskFunc, uint32_t *stackMem, uint32_t stackSize, TaskPriority_e priority);
// Puts a task onto ready queue
bool osStartTask(Task_t *task);
// Forcefully removes task from all of the lists
bool osKillTask(Task_t *task);
// Puts active task into delayed step for a specified time
void osDelay(osTimestamp_t delayMs);
// Trigger a context switch to volunteerly give CPU time to other tasks
void osYield(void);

// Initialises the kernel and loads OS Idle task into the queue
void osInit(void);
// Starts the kernel
void osStart(void);
