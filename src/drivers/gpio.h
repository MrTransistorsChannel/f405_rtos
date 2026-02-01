/*
 * gpio.h
 *
 * A simple macro library for
 * easy control of GPIOs
 * in Arduino style
 *
 *  Created on: 17 apr 2023
 *  Author: Ilya Kobets aka MrTransistor
 */

#pragma once

#include <stm32f4xx.h>

#include "gpio.h"

typedef enum {
    // Port A
    PA0,
    PA1,
    PA2,
    PA3,
    PA4,
    PA5,
    PA6,
    PA7,
    PA8,
    PA9,
    PA10,
    PA11,
    PA12,
    PA13,
    PA14,
    PA15,
    // Port B
    PB0,
    PB1,
    PB2,
    PB3,
    PB4,
    PB5,
    PB6,
    PB7,
    PB8,
    PB9,
    PB10,
    PB11,
    PB12,
    PB13,
    PB14,
    PB15,
    // Port C
    PC0,
    PC1,
    PC2,
    PC3,
    PC4,
    PC5,
    PC6,
    PC7,
    PC8,
    PC9,
    PC10,
    PC11,
    PC12,
    PC13,
    PC14,
    PC15,
    // Port D
    PD0,
    PD1,
    PD2,
    PD3,
    PD4,
    PD5,
    PD6,
    PD7,
    PD8,
    PD9,
    PD10,
    PD11,
    PD12,
    PD13,
    PD14,
    PD15,
    // Port E
    PE0,
    PE1,
    PE2,
    PE3,
    PE4,
    PE5,
    PE6,
    PE7,
    PE8,
    PE9,
    PE10,
    PE11,
    PE12,
    PE13,
    PE14,
    PE15,
    // Port F
    PF0,
    PF1,
    PF2,
    PF3,
    PF4,
    PF5,
    PF6,
    PF7,
    PF8,
    PF9,
    PF10,
    PF11,
    PF12,
    PF13,
    PF14,
    PF15,
    // Port G
    PG0,
    PG1,
    PG2,
    PG3,
    PG4,
    PG5,
    PG6,
    PG7,
    PG8,
    PG9,
    PG10,
    PG11,
    PG12,
    PG13,
    PG14,
    PG15,
    // Port H
    PH0,
    PH1,
    PH2,
    PH3,
    PH4,
    PH5,
    PH6,
    PH7,
    PH8,
    PH9,
    PH10,
    PH11,
    PH12,
    PH13,
    PH14,
    PH15,
    // Port I
    PI0,
    PI1,
    PI2,
    PI3,
    PI4,
    PI5,
    PI6,
    PI7,
    PI8,
    PI9,
    PI10,
    PI11,
    PI12,
    PI13,
    PI14,
    PI15
} GPIO_Pin_e;

#define GPIO_CLK_MASK(Pxn)  (1 << (Pxn / 16))
#define GPIO_PORT(Pxn)      ((GPIO_TypeDef*)(GPIOA_BASE + (Pxn >> 4) * 0x400))
#define GPIO_PIN_NUM(Pxn)   (Pxn & 0xF)
#define GPIO_PIN_MASK(Pxn)  (1 << GPIO_PIN_NUM(Pxn))

/**** GPIO modes ****/
typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_ALT,
    GPIO_MODE_ANALOG
} GPIO_Mode_e;

/**** GPIO speeds ****/
typedef enum {
    GPIO_SPEED_LOW = 0,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERYHIGH
} GPIO_Speed_e;

/**** GPIO pull-up / pull-down****/
typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
} GPIO_Pull_e;

/**** GPIO output mode definitions ****/
typedef enum {
    GPIO_OUTPUT_PUSHPULL = 0,
    GPIO_OUTPUT_OPENDRAIN
} GPIO_OutputType_e;

typedef enum {
    GPIO_AF0_SYS = 0,
    GPIO_AF1_TIM1_2,
    GPIO_AF2_TIM3_4_5,
    GPIO_AF3_TIM8_9_10_11,
    GPIO_AF4_I2C1_2_3,
    GPIO_AF5_SPI1_2,
    GPIO_AF6_SPI3,
    GPIO_AF7_UART1_2_3,
    GPIO_AF8_UART4_5_6,
    GPIO_AF9_CAN1_2_TIM12_13_14,
    GPIO_AF10_USB_OTG,
    GPIO_AF11_ETH,
    GPIO_AF12_FSMC_SDIO,
    GPIO_AF13_DCMI,
    GPIO_AF14,
    GPIO_AF15
} GPIO_AltFunc_e;

typedef enum {
    GPIO_STATE_LOW,
    GPIO_STATE_HIGH
} GPIO_State_e;

__STATIC_FORCEINLINE void GPIO_enableClock(GPIO_Pin_e pin){
    RCC->AHB1ENR |= GPIO_CLK_MASK(pin);
}

__STATIC_FORCEINLINE void GPIO_disableClock(GPIO_Pin_e pin){
    RCC->AHB1ENR &= ~GPIO_CLK_MASK(pin);
}

__STATIC_FORCEINLINE void GPIO_setPinMode(GPIO_Pin_e pin, GPIO_Mode_e mode) {
    GPIO_PORT(pin)->MODER = (GPIO_PORT(pin)->MODER & ~(0x3 << (GPIO_PIN_NUM(pin) * 2))) | (mode << (GPIO_PIN_NUM(pin) * 2));
}

__STATIC_FORCEINLINE void GPIO_setPinSpeed(GPIO_Pin_e pin, GPIO_Speed_e spd) {
    GPIO_PORT(pin)->OSPEEDR = (GPIO_PORT(pin)->OSPEEDR & ~(0x3 << (GPIO_PIN_NUM(pin) * 2))) | (spd << (GPIO_PIN_NUM(pin) * 2));
}

__STATIC_FORCEINLINE void GPIO_setPinPull(uint8_t pin, GPIO_Pull_e pull) {
    GPIO_PORT(pin)->PUPDR = (GPIO_PORT(pin)->PUPDR & ~(0x3 << (GPIO_PIN_NUM(pin) * 2))) | (pull << (GPIO_PIN_NUM(pin) * 2));
}

__STATIC_FORCEINLINE void GPIO_setPinOutputType(uint8_t pin, GPIO_OutputType_e type) {
    GPIO_PORT(pin)->OTYPER = (GPIO_PORT(pin)->OTYPER & ~GPIO_PIN_MASK(pin)) | (type << GPIO_PIN_NUM(pin));
}

__STATIC_FORCEINLINE void GPIO_setPinAltFunc(uint8_t pin, GPIO_AltFunc_e af) {
    if (GPIO_PIN_NUM(pin) < 8)
        GPIO_PORT(pin)->AFR[0] = (GPIO_PORT(pin)->AFR[0] & ~(0xF << (GPIO_PIN_NUM(pin) * 4))) | (af << (GPIO_PIN_NUM(pin) * 4));
    else
        GPIO_PORT(pin)->AFR[1] = (GPIO_PORT(pin)->AFR[1] & ~(0xF << ((GPIO_PIN_NUM(pin) - 8) * 4))) | (af << ((GPIO_PIN_NUM(pin) - 8) * 4));
}

__STATIC_FORCEINLINE uint8_t GPIO_readPin(GPIO_Pin_e pin) {
    return (GPIO_PORT(pin)->IDR >> GPIO_PIN_NUM(pin)) & 0x1;
}

__STATIC_FORCEINLINE void GPIO_writePin(GPIO_Pin_e pin, GPIO_State_e state) {
    GPIO_PORT(pin)->ODR = (GPIO_PORT(pin)->ODR & ~GPIO_PIN_MASK(pin)) | ((state & 0x1) << GPIO_PIN_NUM(pin));
}

__STATIC_FORCEINLINE void GPIO_togglePin(GPIO_Pin_e pin) {
    GPIO_PORT(pin)->ODR ^= GPIO_PIN_MASK(pin);
}
