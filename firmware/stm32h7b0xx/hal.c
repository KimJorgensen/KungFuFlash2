/*
 * Copyright (c) 2019-2024 Kim Jørgensen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "system_stm32h7xx.c"
#include "hal.h"
#include "print.h"
#include "c64_interface.c"
#include "diskio.c"
#include "usb.c"
#include "usart.c"
#include "flash.c"

/******************************************************************************
* Enable caches and the APB4 peripheral clock
* Based on https://github.com/STMicroelectronics/stm32h7xx_hal_driver
******************************************************************************/
static void sys_init(void)
{
    // Force write-through for all cacheable memory regions
    SCB->CACR = SCB_CACR_FORCEWT_Msk;

    SCB_EnableICache();
    SCB_EnableDCache();

    // 4 bits for pre-emption priority 0 bits for subpriority
    NVIC_SetPriorityGrouping(0x03);

    // Enable the APB4 peripheral clock
    SET_BIT(RCC->APB4ENR, RCC_APB4ENR_SYSCFGEN);
    // Delay after enabling clock
    (void)RCC->APB4ENR;
}

/******************************************************************************
* Configure system clock to 280 MHz
* Based on https://github.com/STMicroelectronics/stm32h7xx_hal_driver
******************************************************************************/
static void sysclk_config(void)
{
    // AXI clock gating
    RCC->CKGAENR = 0xFFFFFFFF;

    // Set the power supply configuration
    SET_BIT(PWR->CR3, PWR_CR3_LDOEN);
    // Wait till voltage level flag is set
    while (!(PWR->CSR1 & PWR_CSR1_ACTVOSRDY));

    // Configure the main internal regulator voltage scaling to 0
    MODIFY_REG(PWR->SRDCR, PWR_SRDCR_VOS, PWR_SRDCR_VOS);
    // Delay after setting the voltage scaling
    (void)PWR->SRDCR;
    // Wait till VOS ready bit is set
    while (!(PWR->SRDCR & PWR_SRDCR_VOSRDY));

    // Enable HSE
    SET_BIT(RCC->CR, RCC_CR_HSEON);
    // Wait till HSE is ready
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Disable the main PLL
    CLEAR_BIT(RCC->CR, RCC_CR_PLL1ON);
    // Wait till PLL is disabled
    while (RCC->CR & RCC_CR_PLL1RDY);

    // Configure the main PLL clock source, multiplication and division factors
    MODIFY_REG(RCC->PLLCKSELR,
               (RCC_PLLCKSELR_PLLSRC|RCC_PLLCKSELR_DIVM1),
               RCC_PLLCKSELR_PLLSRC_HSE|(PLL_M1 << RCC_PLLCKSELR_DIVM1_Pos));

    WRITE_REG(RCC->PLL1DIVR, ((PLL_N1 - 1) << RCC_PLL1DIVR_N1_Pos)|
                             ((PLL_P1 - 1) << RCC_PLL1DIVR_P1_Pos)|
                             ((PLL_Q1 - 1) << RCC_PLL1DIVR_Q1_Pos)|
                             ((PLL_R1 - 1) << RCC_PLL1DIVR_R1_Pos));

    // Select PLL1 frequency range: Input 4 to 8 MHz and output 128 to 560 MHz
    // Enable output for System Clock, PLL1Q, and PLL1R
    MODIFY_REG(RCC->PLLCFGR, (RCC_PLLCFGR_PLL1RGE|RCC_PLLCFGR_PLL1VCOSEL),
               (RCC_PLLCFGR_DIVR1EN|RCC_PLLCFGR_DIVQ1EN|
                RCC_PLLCFGR_DIVP1EN|RCC_PLLCFGR_PLL1RGE_2));

    // Enable the main PLL
    SET_BIT(RCC->CR, RCC_CR_PLL1ON);
    // Wait till the main PLL is ready
    while (!(RCC->CR & RCC_CR_PLL1RDY));

    // Set flash read latency and programming delay
    MODIFY_REG(FLASH->ACR, (FLASH_ACR_LATENCY|FLASH_ACR_WRHIGHFREQ),
               (FLASH_ACR_LATENCY_6WS|FLASH_ACR_WRHIGHFREQ));
    (void)FLASH->ACR;

    // Configure the CPU, AHB and APB buses clocks
    MODIFY_REG(RCC->CDCFGR1,
               (RCC_CDCFGR1_CDCPRE|RCC_CDCFGR1_CDPPRE|RCC_CDCFGR1_HPRE),
               (RCC_CDCFGR1_CDCPRE_DIV1|RCC_CDCFGR1_CDPPRE_DIV2|
                RCC_CDCFGR1_HPRE_DIV1));

    MODIFY_REG(RCC->CDCFGR2, RCC_CDCFGR2_CDPPRE1, RCC_CDCFGR2_CDPPRE1_DIV2);
    MODIFY_REG(RCC->CDCFGR2, RCC_CDCFGR2_CDPPRE2, RCC_CDCFGR2_CDPPRE2_DIV2);
    MODIFY_REG(RCC->SRDCFGR, RCC_SRDCFGR_SRDPPRE, RCC_SRDCFGR_SRDPPRE_DIV2);

    // Select the main PLL as system clock source
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL1);
    // Wait till the main PLL is used as system clock source
    while ((RCC->CFGR & RCC_CFGR_SWS) != (RCC_CFGR_SW_PLL1 << RCC_CFGR_SWS_Pos));

    // Update the SystemCoreClock global variable
    SystemCoreClockUpdate();
}

/******************************************************************************
* Configure PLL3Q clock to 48 MHz for USB
* Based on https://github.com/STMicroelectronics/stm32h7xx_hal_driver
******************************************************************************/
static void usbclk_config(void)
{
    // Disable PLL3
    CLEAR_BIT(RCC->CR, RCC_CR_PLL3ON);
    // Wait till PLL3 is disabled
    while (RCC->CR & RCC_CR_PLL3RDY);

    // Configure the PLL3 multiplication and division factors
    MODIFY_REG(RCC->PLLCKSELR, RCC_PLLCKSELR_DIVM3,
               (PLL_M3 << RCC_PLLCKSELR_DIVM3_Pos));

    WRITE_REG(RCC->PLL3DIVR, ((PLL_N3 - 1) << RCC_PLL3DIVR_N3_Pos)|
                             ((PLL_P3 - 1) << RCC_PLL3DIVR_P3_Pos)|
                             ((PLL_Q3 - 1) << RCC_PLL3DIVR_Q3_Pos)|
                             ((PLL_R3 - 1) << RCC_PLL3DIVR_R3_Pos));

    // Select PLL3 frequency range: Input 4 to 8 MHz and output 128 to 560 MHz
    // Enable output for PLL3Q
    MODIFY_REG(RCC->PLLCFGR, (RCC_PLLCFGR_PLL3RGE|RCC_PLLCFGR_PLL3VCOSEL),
               (RCC_PLLCFGR_DIVQ3EN|RCC_PLLCFGR_PLL3RGE_2));

    // Enable PLL3
    SET_BIT(RCC->CR, RCC_CR_PLL3ON);
    // Wait till PLL3 is ready
    while (!(RCC->CR & RCC_CR_PLL3RDY));

    // Set PLL3Q as the USB clock
    MODIFY_REG(RCC->CDCCIP2R, RCC_CDCCIP2R_USBSEL, RCC_CDCCIP2R_USBSEL_1);
}

/******************************************************************************
* System tick timer
******************************************************************************/
static inline void timer_reset(void)
{
    SysTick->VAL = 0;
}

static inline void timer_start_us(u32 us)
{
    SysTick->LOAD = (280 / 8) * us;
    timer_reset();
}

// Max supported value is 479 ms
static inline void timer_start_ms(u32 ms)
{
    timer_start_us(1000 * ms);
}

static inline bool timer_elapsed(void)
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk);
}

static inline void delay_us(u32 us)
{
    timer_start_us(us);
    while (!timer_elapsed());
}

static void delay_ms(u32 ms)
{
    for (u32 i=0; i<ms; i++)
    {
        delay_us(1000);
    }
}

static void systick_config(void)
{
    // Stop timer
    timer_start_us(0);

    // Enable SysTick timer and use HCLK/8 as clock source
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
}

/******************************************************************************
* Floating-point unit
******************************************************************************/
static void fpu_config(void)
{
    // No automatic FPU state preservation
    MODIFY_REG(FPU->FPCCR, FPU_FPCCR_LSPEN_Msk|FPU_FPCCR_ASPEN_Msk, 0);

    // No floating-point context active
    __set_CONTROL(0);
}

/******************************************************************************
* Debug cycle counter
******************************************************************************/
static void dwt_cyccnt_config(void)
{
    // Enable DWT
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;

    // Enable CPU cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*****************************************************************************/
NO_RETURN system_restart(void)
{
    c64_disable();
    filesystem_unmount();
    led_off();

    usart_wait_for_tx();

    NVIC_SystemReset();
    while (true);
}

NO_RETURN restart_to_menu(void)
{
    set_menu_signature();
    system_restart();
}

/*****************************************************************************/
static void configure_system(void)
{
    sys_init();
    sysclk_config();
    fpu_config();
    dwt_cyccnt_config();
    systick_config();

    // Enable GPIOA, GPIOB, GPIOC, GPIOD, and GPIOE clock
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN|RCC_AHB4ENR_GPIOBEN|
                    RCC_AHB4ENR_GPIOCEN|RCC_AHB4ENR_GPIODEN|
                    RCC_AHB4ENR_GPIOEEN;
    (void)RCC->AHB4ENR;

    usb_config();
    usart_config();
    c64_interface_config();
}
