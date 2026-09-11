/*
 * scheduler.c
 *
 *  Created on: Jan 7, 2026
 *      Author: MrTransistor
 */

#include "scheduler.h"

#include <stddef.h>

// Specifies how fast the dynamic priority will grow
const uint8_t taskBaseDynPriority[TASK_PRIORITY_LVL_NUM] = { 0, 1, 2, 3, 255 };

// Task states:
// Ready - tasks actively being executed
// Delayed - tasks waiting for timeout or event
// Suspended - tasks stopped untill resumed
TaskQueue_t readyQueue[TASK_PRIORITY_LVL_NUM];
TaskQueue_t delayedList;
TaskQueue_t suspendedList;

// Pushes a task onto the queue's head
static inline void _queuePushTask(TaskQueue_t *queue, Task_t *task) {
    // Check if both queue and task exist
    if (!queue || !task)
        return;
    // Link task to next
    task->prev = NULL;
    task->next = queue->head;
    // Link next to task
    if (queue->head)
        queue->head->prev = task;
    // Set queue head to this task
    queue->head = task;
    // If the list was empty, also set the last item
    if (queue->tail == NULL)
        queue->tail = task;
}

// Pops a task from the queue's tail
static inline Task_t* _queuePopTask(TaskQueue_t *queue) {
    // Check that the queue exists and is not empty
    if (!queue || !queue->tail)
        return NULL;
    Task_t *tail = queue->tail;
    Task_t *new_tail = tail->prev;
    // Unlink task from previous
    tail->prev = NULL;
    // Unlink previous from task
    if (new_tail)
        new_tail->next = NULL;
    // Move tail to previous
    queue->tail = new_tail;
    // If removed last element
    if (queue->head == tail)
        queue->head = NULL;
    return tail;
}

// Removes a task from the queue
static inline Task_t* _queueRemoveTask(TaskQueue_t *queue, Task_t *task) {
    // Check if both queue and task exist
    if (!queue || !task)
        return NULL;
    // This does not know if a task is in the queue, it can only know if
    // a task is at the head or tail of the queue. Thus it can only successfully
    // remove a task if the queue specified contains this task, otherwise the task
    // will be unlinked from its neighbors, but not from the queue's head or tail

    Task_t *prev = task->prev;
    Task_t *next = task->next;
    // Unlink this task from its neighbors
    task->prev = NULL;
    task->next = NULL;
    // Collapse the list
    if (prev)
        prev->next = next;
    if (next)
        next->prev = prev;
    // Check if removed the head
    if (queue->head == task)
        queue->head = next;
    // Check if removed the tail
    if (queue->tail == task)
        queue->tail = prev;
    return task;
}

void scheduler_init(void) {
    // Reset queue dynamic priorities
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++)
        readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];
}

bool startTask(Task_t *task) {
    // Check if the task exists
    if (!task)
        return false;
    // Check if specified priority is valid
    if (task->staticPriority >= TASK_PRIORITY_LVL_NUM)
        return false;
    // Set task's state to ready
    task->state = TASK_STATE_READY;
    // Push the task into the apropriate ready queue
    _queuePushTask(&readyQueue[task->staticPriority], task);
    return true;
}

bool suspendTask(Task_t *task) {
    // Check if the task exists
    if (!task)
        return false;
    // Get the appropriate list to remove from
    TaskQueue_t *queue;
    switch (task->state) {
        case TASK_STATE_READY:
            queue = &readyQueue[task->staticPriority];
            break;
        case TASK_STATE_DELAYED:
            queue = &delayedList;
            break;
        default:
            return false;
    }
    // Move to the suspended list
    _queueRemoveTask(queue, task);
    _queuePushTask(&suspendedList, task);
    task->state = TASK_STATE_SUSPENDED;
    return true;
}

bool resumeTask(Task_t *task) {
    // Check that the task exists and is suspended
    if (!task || (task->state != TASK_STATE_SUSPENDED))
        return false;
    // Move to the apropriate ready queue
    _queueRemoveTask(&suspendedList, task);
    _queuePushTask(&readyQueue[task->staticPriority], task);
    task->state = TASK_STATE_READY;
    return true;
}

bool killTask(Task_t *task) {
    // Check if the task exists
    if (!task)
        return false;
    // Get the appropriate list to remove from
    TaskQueue_t *queue;
    switch (task->state) {
        case TASK_STATE_READY:
            queue = &readyQueue[task->staticPriority];
            break;
        case TASK_STATE_DELAYED:
            queue = &delayedList;
            break;
        case TASK_STATE_SUSPENDED:
            queue = &suspendedList;
            break;
        default:
            return false;
    }
    _queueRemoveTask(queue, task);
    return true;
}

void schedule() {
    // Cache time
    const timeUs_t timeUs = micros();

    // 1. Check if any tasks need to be put into ready queue
    for (Task_t *task = delayedList.head; task;) {
        // Store because we can't remove an element and then iterate over the list
        Task_t *next = task->next;
        // If timeout expired, move to the appropriate ready queue
        if (timeUs - task->lastExecTimeUs > task->periodUs) {
            _queueRemoveTask(&delayedList, task);
            _queuePushTask(&readyQueue[task->staticPriority], task);
            task->state = TASK_STATE_READY;
        }
        task = next;
    }

    // 2. Find a queue to run from
    TaskQueue_t *activeQueue = NULL;
    TaskPriority_e maxPriority = 0;
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++) {
        TaskPriority_e priority = readyQueue[prio].dynamicPriority;
        // If this queue has a higher priority and is not empty
        // Non-strict comparison needed to allow static priority to decide
        // if the dynamic priorities are equal. Later queues have higer static priority
        if ((priority >= maxPriority) && (readyQueue[prio].tail != NULL)) {
            maxPriority = priority;
            activeQueue = &readyQueue[prio];
        }
    }
    // If no queue was found, all queues are empty
    if (!activeQueue)
        return;

    // 3. Update dynamic priorities
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++) {
        // Active queue has its dynamic priority reset to default value
        // All other queues that are not empty are thus pending and have their dyn priority
        // increased by the base value
        // All empty queues also have their priorities reset
        if (&readyQueue[prio] == activeQueue)
            readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];
        else if (readyQueue[prio].tail != NULL) {
            uint16_t new_prio = readyQueue[prio].dynamicPriority + taskBaseDynPriority[prio];
            readyQueue[prio].dynamicPriority = (new_prio > TASK_DYN_PRIO_LIMIT) ? TASK_DYN_PRIO_LIMIT : (uint8_t) new_prio;
        }
        else
            readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];
    }

    // 4. Execute active task
    Task_t *activeTask = _queuePopTask(activeQueue);
    activeTask->lastExecTimeUs = timeUs;
    activeTask->taskFunc(timeUs);

    // 5. Move active task back to the delayed queue
    _queuePushTask(&delayedList, activeTask);
    activeTask->state = TASK_STATE_DELAYED;
}
