/*
 * scheduler.h
 *
 *  Created on: Jan 7, 2026
 *      Author: MrTransistor
 */

#pragma once

#include <stdbool.h>

#include "common/time.h"

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

// This is set to one below the realtime priority to allow realtime tasks to
// allways take priority
#define TASK_DYN_PRIO_LIMIT     254

// Macro definitions for common period conversions
#define TASK_FREQ_HZ(hz) (1000000 / (hz))
#define TASK_PERIOD_MS(ms) ((ms) * 1000)
#define TASK_PERIOD_US(us) (us)

typedef struct task_s {
    // Linkage pointers
    struct task_s *prev;
    struct task_s *next;

    const void (*taskFunc)(timeUs_t execTimeUs);

    timeDelta_t periodUs;
    timeUs_t lastExecTimeUs;
    timeDelta_t latestDeltaTimeUs;

    TaskState_e state;
    const uint8_t staticPriority;
} Task_t;

typedef struct {
    uint8_t dynamicPriority;
    Task_t *head;
    Task_t *tail;
} TaskQueue_t;

void scheduler_init(void);
bool startTask(Task_t *task);
bool suspendTask(Task_t *task);
bool resumeTask(Task_t *task);
bool killTask(Task_t *task);
void schedule();
