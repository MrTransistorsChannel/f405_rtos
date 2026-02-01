/*
 * scheduler.c
 *
 *  Created on: Jan 8, 2026
 *      Author: MrTransistor
 */

#include "scheduler.h"

#include <stddef.h>
#include <string.h>

// Specifies how fast the dynamic priority will grow
const uint8_t taskBaseDynPriority[TASK_PRIORITY_LVL_NUM] = { 0, 1, 2, 3, 255 };

// Task states:
// Ready - tasks actively being executed
// Delayed - tasks waiting for timeout or event
static TaskReadyQueue_t readyQueue[TASK_PRIORITY_LVL_NUM];
static TaskList_t delayedList;

// Currentrly running task. Used by the context switch logic
// updated by scheduler
static Task_t *activeTask = NULL;

//############## OS IDLE TASK ##############
// Must be aligned to 8 bytes (ARM EABI requirement)
uint32_t __attribute__((aligned(8))) osIdleTaskStack[OS_IDLE_TASK_STACK_SIZE];

static void osIdleTaskFunc(void) {
    // Do nothing for now. Can add system statistics later
    // Test the task entry
    while (1);
    //osYield();  // Constantly yield for other processes
}

// Runs at minimum priority level with other idle tasks. Never finishes
static Task_t osIdleTask;

// Helper functions. Require arguments to be not NULL
// Add a task to the end of the list
static inline void _listAddTask(TaskList_t *list, Task_t *task) {
    // Link task to list
    task->prev = list->tail;
    task->next = NULL;
    // Link list to task
    if (list->tail)
        list->tail->next = task;
    // Set list tail to this task
    list->tail = task;
    // If the list was empty, also set the head
    if (list->head == NULL)
        list->head = task;
}

// Remove a task from the list
static inline Task_t* _listRemoveTask(TaskList_t *list, Task_t *task) {
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
    if (list->head == task)
        list->head = next;
    // Check if removed the tail
    if (list->tail == task)
        list->tail = prev;
    return task;
}

// Push a task to the head of the queue (before the iterator)
static inline void _readyQueuePushTask(TaskReadyQueue_t *queue, Task_t *task) {
    // If the queue was empty
    if (!queue->iter) {
        queue->iter = task;
        // Link the task to itself
        task->prev = task;
        task->next = task;
    }
    else {
        Task_t *next = queue->iter;
        Task_t *prev = next->prev;  // Guranteed to not be NULL (at least one task exists)
        // Link task
        task->prev = prev;
        task->next = next;
        // Link neighbors
        next->prev = task;
        prev->next = task;
    }
}

// Remove a task from the queue. Used to kill or delay a task
static inline Task_t* _readyQueueRemoveTask(TaskReadyQueue_t *queue, Task_t *task) {
    Task_t *next = task->next;
    Task_t *prev = task->prev;
    // If about to empty the queue (contains only one task linked to itself)
    // Don't need to check task->prev == task, assuming the queue was built correctly
    if (next == task)
        queue->iter = NULL;
    else {
        // Collapse the queue around the task
        // next and prev are guranteed to be not equal
        // to the task itself (more than one task exists)
        next->prev = prev;
        prev->next = next;
        // Move iterator to the next task
        queue->iter = next;
    }
    // Unlink the task
    task->next = NULL;
    task->prev = NULL;
    return task;
}

static inline Task_t* _readyQueueIterateForwards(TaskReadyQueue_t *queue) {
    Task_t *task = queue->iter;
    if (task)
        queue->iter = task->next;
    return task;
}

static inline Task_t* _readyQueueIterateBackwards(TaskReadyQueue_t *queue) {
    Task_t *task = queue->iter;
    if (task)
        queue->iter = task->prev;
    return task;
}

// Executes if a task returns
static void __attribute__((noinline)) taskExitHandler(void) {
    osKillTask(activeTask);
    osYield();
}

void osInitTask(Task_t *task, TaskFunc_t taskFunc, uint32_t *stackMem, uint32_t stackSize, TaskPriority_e priority) {
    // Check that the TCB, task function and task stack memory exists
    if (!task || !taskFunc || !stackMem)
        return;

    // Configure the stack
    task->stackTop = stackMem;                  // First word in the stack memory
    task->stackBase = stackMem + stackSize - 1; // Last word in the stack memory

    // Fill task stack memory with a known value for easier debugging
    (void) memset(stackMem, 0xA5, stackSize * sizeof(uint32_t));

    // Simulate an exception entry stack frame:
    task->stackPointer = task->stackBase;                           // Initial stack pointer value, predecremented by one
    *(task->stackPointer--) = 0x01000000UL;                         // Initial xPSR: only Thumb mode bit set
    *(task->stackPointer--) = ((uint32_t) taskFunc) & 0xfffffffeUL; // Initial PC: task entry point with Thumb bit cleared
    *(task->stackPointer--) = (uint32_t) &taskExitHandler;          // Initial LR: task exit handler
    task->stackPointer -= 5;                                        // Skip register init (r12, r3, r2, r1, r0)
    *(task->stackPointer--) = 0xfffffffdUL;                         // Copy of the EXC_RETURN value used by the context switch logic
    task->stackPointer -= 7;                                        // Skip callee-saved register init (r11, r10, r9, r8, r7, r6, r5, r4)

    // Configure the priority
    task->staticPriority = priority;
}

bool osStartTask(Task_t *task) {
    // Check if the task exists
    if (!task)
        return false;
    // Check if specified priority is valid
    if (task->staticPriority > TASK_PRIORITY_LVL_NUM)
        return false;
    // Set task's state to ready
    task->state = TASK_STATE_READY;
    // Push the task into the apropriate ready queue
    _readyQueuePushTask(&readyQueue[task->staticPriority], task);
    return true;
}

bool osKillTask(Task_t *task) {
    // Check if the task exists
    if (!task)
        return false;
    switch (task->state) {
        case TASK_STATE_READY:
            _readyQueueRemoveTask(&readyQueue[task->staticPriority], task);
            osYield();
            return true;

        case TASK_STATE_DELAYED:
            _listRemoveTask(&delayedList, task);
            osYield();
            return true;

        default:
            return false;
    }
}

inline void osYield(void) {
    // If time slicing is disabled, task queues must be manually rotated when yielding
#if (OS_USE_TIME_SLICING == 0)
    _readyQueueIterateForwards(&readyQueue[activeTask->staticPriority]);
#endif
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

// TODO: this whould probably be wrapped in a critical section to prevent systick interrupt from switching context during task deletion
// Or maybe just cache the task in a local variable
void osDelay(osTimestamp_t delayMs) {
    osTimestamp_t timeUs = osTime();
    // Calculate next execution time
    activeTask->nextExecTimeUs = timeUs + delayMs * 1000;
    // Move to the delayed list
    _readyQueueRemoveTask(&readyQueue[activeTask->staticPriority], activeTask);
    _listAddTask(&delayedList, activeTask);
    activeTask->state = TASK_STATE_DELAYED;
    // Perform a context switch
    osYield();
}

void osInit(void) {
    // Reset queue dynamic priorities
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++)
        readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];

    // Initialise the TCB for OS Idle task
    osInitTask(&osIdleTask, &osIdleTaskFunc, osIdleTaskStack, OS_IDLE_TASK_STACK_SIZE, TASK_PRIORITY_IDLE);
    osStartTask(&osIdleTask);
    activeTask = &osIdleTask;   // Force load the task as active for the context switch to pick it up
}

void osStart(void) {
    // Enable FPU (if not already enabled)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    // Enable lazy saving of the FPU context    !Important!
    FPU->FPCCR |= FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;

    // Set PendSV and SVCall to lowest priority
    NVIC_SetPriority(PendSV_IRQn, 15);
    NVIC_SetPriority(SVCall_IRQn, 15);

    // Enable system tick timer
    timebase_init();

    // Clean up the kernel stack and trigger SVC to bootstrap task processing
    asm volatile(
            // Hard-reset kernel stack
            "   ldr r0, =0xe000ed08     \n"// Load address of the SCB->VTOR
            "   ldr r0, [r0]            \n"// Get the SCB->VTOR value (pointer to the vector table)
            "   ldr r0, [r0]            \n"// Get the value of the vTable[0] - initial stack pointer value
            "   msr msp, r0             \n"// Reset main stack pointer to the initial value
            // Clear FPCA bit to prevent FPU context from being saved if used before the kernel start
            "   mov r0, #0              \n"
            "   msr control, r0         \n"
            // Enable interrupts (clear PRIMASK and FAULTMASK)
            "   cpsie i                 \n"
            "   cpsie f                 \n"
            "   dsb                     \n"
            "   isb                     \n"
            // Trigger SVCall
            "   svc 0                   \n"
            "   nop                     \n"
            "   .ltorg                  \n"// Dump literal pool here
    );

    // Shouldn't get here.
    asm volatile("bkpt");
    for (;;);
}

// Run the scheduling algorithm and return a task to run
static inline Task_t* schedule(void) {
    const osTimestamp_t timeUs = osTime();

    // 1. Check if any tasks need to be put into ready queue
    for (Task_t *task = delayedList.head; task;) {
        Task_t *next = task->next;
        // If timeout expired, move to the ready queue
        if (TIME_A_AFTER_B(timeUs, task->nextExecTimeUs)) {
            _listRemoveTask(&delayedList, task);
            _readyQueuePushTask(&readyQueue[task->staticPriority], task);
            task->state = TASK_STATE_READY;
        }
        task = next;
    }

    // 2. Find a queue to run from
    TaskReadyQueue_t *activeQueue = NULL;
    TaskPriority_e maxPriority = 0;
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++) {
        TaskPriority_e priority = readyQueue[prio].dynamicPriority;
        // If this queue has a higher priority and is not empty
        // Non-strict comparison needed to allow static priority to decide
        // if the dynamic priorities are equal. Later queues have higer static priority
        if ((priority >= maxPriority) && (readyQueue[prio].iter)) {
            maxPriority = priority;
            activeQueue = &readyQueue[prio];
        }
    }
    // If no queue was found, all queues are empty
    if (!activeQueue)
        return NULL;

    // 3. Update dynamic priorities
    for (TaskPriority_e prio = 0; prio < TASK_PRIORITY_LVL_NUM; prio++) {
        // Active queue has its dynamic priority reset to default value
        // All other queues that are not empty (and thus are pending) have their dyn priority
        // increased by the base value
        // All empty queues also have their priorities reset
        if (&readyQueue[prio] == activeQueue)
            readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];
        else if (readyQueue[prio].iter) {
            uint16_t new_prio = readyQueue[prio].dynamicPriority + taskBaseDynPriority[prio];
            readyQueue[prio].dynamicPriority = (new_prio > TASK_DYN_PRIO_LIMIT) ? TASK_DYN_PRIO_LIMIT : (uint8_t) new_prio;
        }
        else
            readyQueue[prio].dynamicPriority = taskBaseDynPriority[prio];
    }

    // 4. Return a task to run
#if (OS_USE_TIME_SLICING == 1)
    return _readyQueueIterateForwards(activeQueue); // Move the queue forward to enable time slicing
#elif
    return activeQueue->iter;                       // Return the task with highest priority without rotating the queue
#endif
}

// Switch to the next task or to the idle task if no tasks are active
static void __attribute__((used)) switchContext(void) {
    // Run scheduler
    // activeTask will never be null as there will allways be at least OS Idle task
    activeTask = schedule();
}

// Bootstrap handler for launching the first task
void __attribute__((naked)) SVC_Handler(void) {
    asm volatile(
            // Restore the first task's context without saving interrupted context
            "   ldr r0, =activeTask             \n"// Load address of the activeTask pointer
            "   ldr r0, [r0]                    \n"// Get activeTask value (pointer to the active TCB)
            "   ldr r0, [r0]                    \n"// Get the pointer to the top of the stack stored in the TCB
            "   ldmia r0!, {r4-r11, lr}         \n"// Unload the conext switch stack frame (r4-r11, EXC_RETURN)
            "   msr psp, r0                     \n"// Restore task stack pointer into the PSP
            "   isb                             \n"
            "   mov r0, #0                      \n"// Clear BASEPRI just in case
            "   msr basepri, r0                 \n"
            "   bx lr                           \n"// Trigger exception return (exits to the task entry point)
            "   .ltorg                          \n"// Dump literal pool
    );
}

// Handles the context switch between tasks
void __attribute__((naked)) PendSV_Handler(void) {
    asm("nop");
    asm volatile(
            // Store context of the active task
            "   mrs r0, psp                     \n"// Read current task stack pointer into r0
            "   isb                             \n"

            "   ldr r1, =activeTask             \n"// Load current task stack top address
            "   ldr r2, [r1]                    \n"// from its TCB

            "   tst lr, #0x10                   \n"// Check if current task used FPU
            "   it eq                           \n"// Store FPU context to the process stack
            "   vstmdbeq r0!, {s16-s31}         \n"
            "   stmdb r0!, {r4-r11, lr}         \n"// Save callee-saved registers and EXC_RETURN to be able to examine its value later
            "   str r0, [r2]                    \n"// Store new stack top into current task TCB
            // Call switchContext() to update activeTask
            "   stmdb sp!, {r0, r1}             \n"// Store r1 into main stack. r0 is used to ensure 8 byte alignment which is required by ARM EABI
            "   mov r0, %0                      \n"// Set BASEPRI to a specified minimum level to prevent low priority interrupts from nesting
            "   msr basepri, r0                 \n"
            "   dsb                             \n"// Add data and instruction barriers to prevent pipeline collisions
            "   isb                             \n"
            "   bl switchContext                \n"// Call context switching function
            "   mov r0, #0                      \n"// Reset BASEPRI to zero
            "   msr basepri, r0                 \n"
            "   ldmia sp!, {r0, r1}             \n"// Restore activeTask address in r1
            // Load context of the new task
            "   ldr r0, [r1]                    \n"// Load address of the active TCB by dereferencing activeTask
            "   ldr r0, [r0]                    \n"// Get the active task stack pointer
            "   ldmia r0!, {r4-r11, lr}         \n"// Restore callee-saved registers and EXC_RETURN of the new task
            "   tst lr, #0x10                   \n"// Test if new task was using FPU
            "   it eq                           \n"// Restore FPU context if needed
            "   vldmiaeq r0!, {s16-s31}         \n"

            "   msr psp, r0                     \n"// Set process stack pointer to the value in r0
            "   isb                             \n"

            "   bx lr                           \n"// Trigger exception return
            "   .ltorg                          \n"// Dump literal pool here
            ::"i" (MAX_SYSCALL_INTERRUPT_PRIORITY)
    );
}
