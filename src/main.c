#include <stm32f4xx.h>

#include "drivers/gpio.h"
#include "drivers/usb_vcp/usb_vcp.h"

#include "console/console.h"
#include "scheduler_v2/scheduler.h"

#define PWM1    PC9
#define PWM2    PC8
#define USB_DP  PA12
#define USB_DM  PA11

void GPIO_init(void) {
    GPIO_enableClock(PWM1);
    GPIO_setPinMode(PWM1, GPIO_MODE_OUTPUT);
    GPIO_enableClock(PWM2);
    GPIO_setPinMode(PWM2, GPIO_MODE_OUTPUT);

    // USB
    GPIO_enableClock(USB_DP);
    GPIO_setPinMode(USB_DP, GPIO_MODE_ALT);
    GPIO_setPinAltFunc(USB_DP, GPIO_AF10_USB_OTG);
    GPIO_setPinSpeed(USB_DP, GPIO_SPEED_HIGH);
    GPIO_enableClock(USB_DM);
    GPIO_setPinMode(USB_DM, GPIO_MODE_ALT);
    GPIO_setPinAltFunc(USB_DM, GPIO_AF10_USB_OTG);
    GPIO_setPinSpeed(USB_DM, GPIO_SPEED_HIGH);
}

size_t VCP_readAvailable(void *buf, size_t maxLen) {
    size_t nBytes = VCP_available();
    if (nBytes > maxLen)
        nBytes = maxLen;
    VCP_readBytes(buf, nBytes);
    return nBytes;
}

uint32_t __attribute__((aligned(8))) task1Stack[4096];
uint32_t __attribute__((aligned(8))) task2Stack[128];
Task_t task1;
Task_t task2;
void exampleTask1(void) {
    while (1) {
        if (VCP_available())
            console_process();
    }
}

void exampleTask2(void) {
    while (1) {
        GPIO_writePin(PWM2, 1);
        GPIO_writePin(PWM2, 0);
    }
}

int main(void) {
    GPIO_init();
    USB_init();
    ConsoleConfig_t config = {
            .read = &VCP_readAvailable,
            .write = &VCP_writeBytes,
            .parameters = NULL,
            .param_count = 0,
            .commands = NULL,
            .command_count = 0
    };
    console_init(&config);

    osInit();
    osInitTask(&task1, &exampleTask1, task1Stack, 4096, TASK_PRIORITY_LOW);
    osInitTask(&task2, &exampleTask2, task2Stack, 128, TASK_PRIORITY_HIGH);
    osStartTask(&task1);
    osStartTask(&task2);
    osStart();
    asm("bkpt");
    for (;;);
}

