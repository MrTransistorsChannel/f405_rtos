/*
 * system_stm32f4xx.c
 *
 *  Created on: Mar 21, 2025
 *      Author: MrTransistor
 */

#include <stm32f4xx.h>

void SystemInit(void) {
    // 8MHz HSE, 168MHz Hclk, 3.3V, CSS enabled, FPU enabled

    // ################# CLOCK CONFIGURATION #################

    // For voltage range of 2.7 - 3.6V add 1 WS for each 30 MHz of Hclk
    // For voltage range of 2.4 - 2.7V add 1 WS for each 24 MHz of Hclk
    // For voltage range of 2.1 - 2.4V add 1 WS for each 22 MHz of Hclk
    // For voltage range of 1.8 - 2.1V disable prefetch and add 1 WS for each 20 MHz of Hclk
    MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_5WS);
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_5WS);

    // Enable prefetch if at least 1 WS needed to access flash memory
    // Enable intruction and data caching for flash memory
    FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    // Power scale 1 (default) for Hclk above 144 MHz
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;
    while (!(PWR->CSR & PWR_CSR_VOSRDY));

    // Enable HSE oscillator
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Enable clock security system
    RCC->CR |= RCC_CR_CSSON;

    // Configure and enable PLL for 168 MHz output and 48MHz USB clock
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLM, 4 << RCC_PLLCFGR_PLLM_Pos);
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLN, 168 << RCC_PLLCFGR_PLLN_Pos);
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLP, 0 << RCC_PLLCFGR_PLLM_Pos);  // PLLP = 2
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLQ, 7 << RCC_PLLCFGR_PLLQ_Pos);
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLSRC, RCC_PLLCFGR_PLLSRC_HSE);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // Configure clock bus prescalers
    MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2, RCC_CFGR_HPRE_DIV1 |
            RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2);

    // Switch Hclk to PLL output
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // #######################################################

    //Enable FPU coprocessor full access
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
}
