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

#include "c64_interface.h"

/******************************************************************************
* C64 address bus on PE0-PE15
******************************************************************************/
static void c64_address_config(void)
{
    // Make GPIOE input
    C64_ADDR_INPUT();

    // Set output low
    GPIOE->ODR = 0;

    // Set GPIOE to low speed
    GPIOE->OSPEEDR = 0;

    // No pull-up, pull-down
    GPIOE->PUPDR = 0;
}

/******************************************************************************
* C64 data bus on PD0-PD7
******************************************************************************/
static void c64_data_config(void)
{
    // Make PD0-PD7 input
    C64_DATA_INPUT();
}

/******************************************************************************
* C64 control bus on PB0-PB2 and PB10-PB12
******************************************************************************/
static void c64_control_config(void)
{
    // Release write signal PB0
    C64_CONTROL_WRITE(C64_WRITE_HIGH);

    // Set PB0 as open-drain
    GPIOB->OTYPER |= GPIO_OTYPER_OT0;

    // Make PB0 output
    MODIFY_REG(GPIOB->MODER, GPIO_MODER_MODE0, GPIO_MODER_MODE0_0);

    // Make PB1-PB2 and PB10-PB12 input
    GPIOB->MODER &= ~(GPIO_MODER_MODE1|GPIO_MODER_MODE2|GPIO_MODER_MODE10|
                      GPIO_MODER_MODE11|GPIO_MODER_MODE12);

    // No pull-up, pull-down on PB0-PB2 and PB10-PB12
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD0|GPIO_PUPDR_PUPD1|GPIO_PUPDR_PUPD2|
                      GPIO_PUPDR_PUPD10|GPIO_PUPDR_PUPD11|GPIO_PUPDR_PUPD12);
}

// Wait until the raster beam is in the upper or lower border (if VIC-II is enabled)
static void c64_sync_with_vic(void)
{
    timer_start_us(1000);
    while (!timer_elapsed())
    {
        if (!(C64_CONTROL_READ() & C64_BA))
        {
            timer_reset();
        }
    }
}

/******************************************************************************
* C64 IRQ and NMI on PA2 & PA3
* C64 DMA, GAME, and EXROM on PA7 & PA9-PA10
* Status LED on PA15
******************************************************************************/
static void c64_crt_config(void)
{
    // Cartridge inactive
    C64_CRT_CONTROL(STATUS_LED_OFF|C64_IRQ_NMI_HIGH|C64_DMA_HIGH|C64_GAME_HIGH|
                    C64_EXROM_HIGH);

    // Set PA2-PA3 & PA7 & PA9-PA10 as open-drain
    GPIOA->OTYPER |= GPIO_OTYPER_OT2|GPIO_OTYPER_OT3|GPIO_OTYPER_OT7|
                     GPIO_OTYPER_OT9|GPIO_OTYPER_OT10;

    // Set PA2-PA3 & PA7 & PA9-PA10 as output
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODE2|GPIO_MODER_MODE3|
                             GPIO_MODER_MODE7|GPIO_MODER_MODE9|
                             GPIO_MODER_MODE10|GPIO_MODER_MODE15,
                             GPIO_MODER_MODE2_0|GPIO_MODER_MODE3_0|
                             GPIO_MODER_MODE7_0|GPIO_MODER_MODE9_0|
                             GPIO_MODER_MODE10_0|GPIO_MODER_MODE15_0);
}

/******************************************************************************
* C64 clock input on PA8 (timer 1)
* C64 clock is 0.985 MHz (PAL) / 1.023 MHz (NTSC)
******************************************************************************/
static void c64_clock_config()
{
    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;

    // Set PA8 as alternate mode 1 (TIM1_CH1)
    MODIFY_REG(GPIOA->AFR[1], GPIO_AFRH_AFSEL8, GPIO_AFRH_AFSEL8_0);
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODE8, GPIO_MODER_MODE8_1);

    // No prescaler, timer runs at ABP2 timer clock speed (168 MHz)
    TIM1->PSC = 0;

    /**** Setup timer 1 to measure clock speed in CCR1 and duty cycle in CCR2 ****/

    // CC1 and CC2 channel is input, IC1 and IC2 is mapped on TI1
    TIM1->CCMR1 = TIM_CCMR1_CC1S_0|TIM_CCMR1_CC2S_1;

    // TI1FP1 active on falling edge. TI1FP2 active on rising edge
    TIM1->CCER = TIM_CCER_CC1P;

    // Select TI1FP1 as trigger
    TIM1->SMCR = TIM_SMCR_TS_2|TIM_SMCR_TS_0;

    // Set reset mode
    TIM1->SMCR |= TIM_SMCR_SMS_2;

    // Enable capture 1 and 2
    TIM1->CCER |= TIM_CCER_CC1E|TIM_CCER_CC2E;

    /**** Setup phi2 interrupt ****/

    // Generate CC3/4 interrupt just before the C64 CPU clock cycle begings
    // Value set by c64_interface()
    TIM1->CCR3 = 87;
    TIM1->CCR4 = 87;

    // Enable compare mode 1. OC3REF/4 is high when TIM1_CNT == TIM1_CCR3/4
    TIM1->CCMR2 = TIM_CCMR2_OC3M_0|TIM_CCMR2_OC4M_0;

    // Disable all TIM1 interrupts
    TIM1->DIER = 0;

    // Enable TIM1_CC_IRQn, highest priority
    NVIC_SetPriority(TIM1_CC_IRQn, 0);
    NVIC_EnableIRQ(TIM1_CC_IRQn);

    // Enable counter
    TIM1->CR1 = TIM_CR1_CEN;
}

/******************************************************************************
* C64 diagnostic
******************************************************************************/
#define DIAG_INIT   (0x00)
#define DIAG_RUN    (0x01)
#define DIAG_STOP   (0x02)

static volatile u32 diag_state;
static volatile u32 diag_phi2_freq;

static void c64_diag_handler(void)
{
    if (diag_state == DIAG_RUN)
    {
        // Count number of interrupts within 1 second
        if (DWT->CYCCNT < SystemCoreClock)
        {
            // Check for phi2 clock (no timer overflow)
            if (TIM1->SR & TIM_SR_CC1IF)
            {
                diag_phi2_freq++;
            }
        }
        else
        {
            diag_state = DIAG_STOP;
        }
    }
    else if (diag_state == DIAG_INIT)
    {
        // Start measuring
        DWT->CYCCNT = 0;
        diag_phi2_freq = 0;
        diag_state = DIAG_RUN;
    }

    // Clear the interrupt flag
    TIM1->SR = 0;

    // Ensure interrupt flag is cleared
    (void)TIM1->SR;
}

/******************************************************************************
* C64 interface
******************************************************************************/
static void c64_wait_valid_clock(void)
{
    u32 valid_clock_count = 0;
    u32 led_activity = 0;

    // Wait for a valid C64 clock signal
    while (cfg_file.boot_type != CFG_DIAG && valid_clock_count < 100)
    {
        // NTSC: 271-272, PAL: 282-283
        if (!(TIM1->SR & TIM_SR_CC1IF) || TIM1->CCR1 < 270 || TIM1->CCR1 > 284)
        {
            valid_clock_count = 0;
        }
        else
        {
            valid_clock_count++;
        }

        // Blink rapidly if no valid clock
        if (led_activity++ > 25000)
        {
            led_activity = 0;
            led_toggle();
        }

        delay_us(2); // Wait more than a clock cycle
    }
    led_on();
}

static void c64_interface_enable_no_config(void)
{
    __disable_irq();
    // Wait for the next interrupt before the C64 bus handler is enabled
    TIM1->SR = 0;
    while (!(TIM1->SR & TIM_SR_CC3IF));

    // Capture/Compare 3 interrupt enable
    TIM1->SR = 0;
    TIM1->DIER = TIM_DIER_CC3IE;
    __enable_irq();
}

static void c64_interface(bool state)
{
    if (!state)
    {
        C64_INTERFACE_DISABLE();
        return;
    }

    if (c64_interface_active())
    {
        return;
    }

    c64_wait_valid_clock();

    // Configure the phi2 offset
    s8 phi2_offset = cfg_file.phi2_offset;

    // Limit offset
    if (phi2_offset > 30)
    {
        phi2_offset = 30;
    }
    else if (phi2_offset < -30)
    {
        phi2_offset = -30;
    }
    cfg_file.phi2_offset = phi2_offset;

    if (c64_is_ntsc())  // NTSC timing
    {
        // Generate interrupt before phi2 is high
        TIM1->CCR3 = NTSC_PHI2_INT + phi2_offset;
        TIM1->CCR4 = NTSC_PHI2_INT_DMA + phi2_offset;

        // Abuse COMPx registers for better performance
        DWT->COMP0 = NTSC_PHI2_HIGH + phi2_offset;      // After phi2 is high
        DWT->COMP1 = NTSC_PHI2_LOW + phi2_offset;       // Before phi2 is low
        DWT->COMP2 = NTSC_PHI2_HIGH_DMA + phi2_offset;  // Before phi2 is high
    }
    else    // PAL timing
    {
        // Generate interrupt before phi2 is high
        TIM1->CCR3 = PAL_PHI2_INT + phi2_offset;
        TIM1->CCR4 = PAL_PHI2_INT_DMA + phi2_offset;

        // Abuse COMPx registers for better performance
        DWT->COMP0 = PAL_PHI2_HIGH + phi2_offset;       // After phi2 is high
        DWT->COMP1 = PAL_PHI2_LOW + phi2_offset;        // Before phi2 is low
        DWT->COMP2 = PAL_PHI2_HIGH_DMA + phi2_offset;   // Before phi2 is high
    }
    DWT->COMP3 = -phi2_offset;

    // Start the C64
    c64_reset_release();
    c64_interface_enable_no_config();
}

/******************************************************************************
* C64 reset on PA1
******************************************************************************/
static void c64_reset(void)
{
    c64_reset_detect_disable();

    GPIOA->BSRR = GPIO_BSRR_BS1;
    delay_us(200); // Make sure that the C64 is reset
}

static void c64_reset_release(void)
{
    // Try prevent triggering bug in H.E.R.O. No effect at power-on though
    if (cfg_file.boot_type == CFG_CRT)
    {
        c64_sync_with_vic();
    }

    GPIOA->BSRR = GPIO_BSRR_BR1;

    // Wait until the C64 reset line is released
    if (cfg_file.boot_type != CFG_DIAG)
    {
        while (!(C64_CONTROL_READ() & C64_RESET));
        c64_reset_detect_enable();
    }
}

static inline bool c64_is_reset(void)
{
    return (GPIOA->ODR & GPIO_ODR_OD1) != 0;
}

static inline void c64_enable(void)
{
    c64_interface(true);
}

static void c64_disable(void)
{
    c64_interface(false);
    c64_reset();
}

static void c64_reset_config(void)
{
    c64_reset();

    // Set PA1 as output
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODE1, GPIO_MODER_MODE1_0);
}

/******************************************************************************
* Menu button and special button on PB7 & PB8
* C64 reset detect on PB9
******************************************************************************/
static u32 button_pressed(void)
{
    u32 control = 0;
    for (u32 i=0; i<24; i++)    // Debounce
    {
        control |= C64_CONTROL_READ();
        delay_us(500);
    }

    return control & (MENU_BTN|SPECIAL_BTN);
}

static inline bool menu_button_pressed(void)
{
    return (button_pressed() & MENU_BTN) != 0;
}

static inline bool special_button_pressed(void)
{
    return (button_pressed() & SPECIAL_BTN) != 0;
}

void EXTI9_5_IRQHandler(void)
{
    // Menu button pressed
    if (EXTI->PR1 & EXTI_PR1_PR7)
    {
        restart_to_menu();
    }

    // C64 reset detected
    system_restart();
}

static void menu_button_enable(void)
{
    // Enable PB7 interrupt
    EXTI->PR1 = EXTI_PR1_PR7;
    EXTI->IMR1 |= EXTI_IMR1_IM7;
}

static void button_config(void)
{
    // Make PB7-PB9 input
    GPIOB->MODER &= ~(GPIO_MODER_MODE7|GPIO_MODER_MODE8|GPIO_MODER_MODE9);

    // Enable pull-down on PB7 and PB8
    MODIFY_REG(GPIOB->PUPDR, GPIO_PUPDR_PUPD7|GPIO_PUPDR_PUPD8,
                             GPIO_PUPDR_PUPD7_1|GPIO_PUPDR_PUPD8_1);

    // Rising edge trigger on PB7 and falling edge trigger on PB9
    MODIFY_REG(EXTI->RTSR1, EXTI_RTSR1_TR7|EXTI_RTSR1_TR9, EXTI_RTSR1_TR7);
    MODIFY_REG(EXTI->FTSR1, EXTI_FTSR1_TR7|EXTI_FTSR1_TR9, EXTI_FTSR1_TR9);

    // EXTI7 interrupt on PB7
    MODIFY_REG(SYSCFG->EXTICR[1], SYSCFG_EXTICR2_EXTI7, SYSCFG_EXTICR2_EXTI7_PB);

    // EXTI9 interrupt on PB9
    MODIFY_REG(SYSCFG->EXTICR[2], SYSCFG_EXTICR3_EXTI9, SYSCFG_EXTICR3_EXTI9_PB);

    // Enable EXTI9_5_IRQHandler, lowest priority
    NVIC_SetPriority(EXTI9_5_IRQn, 0x0f);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/******************************************************************************
* Configure C64 interface
******************************************************************************/
static void c64_interface_config(void)
{
    c64_address_config();
    c64_data_config();
    c64_control_config();

    c64_crt_config();
    c64_reset_config();
    c64_clock_config();

    button_config();
}
