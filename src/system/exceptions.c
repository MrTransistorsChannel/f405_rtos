/*
 * exceptions.c
 *
 *  Created on: Jan 4, 2026
 *      Author: MrTransistor
 */

void NMI_Handler(void){
    asm("bkpt");
    for(;;);
}

void HardFault_Handler(void) {
    asm("bkpt");
    for(;;);
}

void MemManage_Handler(void) {
    asm("bkpt");
    for(;;);
}

void BusFault_Handler(void) {
    asm("bkpt");
    for(;;);
}

void UsageFault_Handler(void) {
    asm("bkpt");
    for(;;);
}
