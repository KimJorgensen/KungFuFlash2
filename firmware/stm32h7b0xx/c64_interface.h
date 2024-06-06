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

/******************************************************************************
* C64 address bus on PE0-PE15
* Returned as 32 bit value for performance
******************************************************************************/
#define C64_ADDR_READ() (GPIOE->IDR)

#define C64_ADDR_WRITE(addr)                    \
    /* Make PE0-PE15 outout */                  \
    GPIOE->MODER = 0x55555555;                  \
    GPIOE->ODR = addr

#define C64_ADDR_INPUT()                        \
    /* Make PE0-PE15 input */                   \
    GPIOE->MODER = 0

/******************************************************************************
* C64 data bus on PD0-PD7
* Returned as 32 bit value for performance
******************************************************************************/
#define C64_DATA_READ() (GPIOD->IDR)

#define C64_DATA_WRITE(data)                    \
    /* Make PD0-PD7 outout */                   \
    *((volatile u16 *)&GPIOD->MODER) = 0x5555;  \
    *((volatile u8 *)&GPIOD->ODR) = (data);     \
    COMPILER_BARRIER()

#define C64_DATA_INPUT()                        \
    /* Make PD0-PD7 input */                    \
    *((volatile u16 *)&GPIOD->MODER) = 0x0000;  \
    COMPILER_BARRIER()

/******************************************************************************
* C64 control bus on PB0-PB2 and PB10-PB12, and C64 reset detect on PB9
* Menu button & special button on PB7 & PB8
* Returned as 32 bit value for performance
******************************************************************************/
#define C64_WRITE_HIGH  GPIO_BSRR_BS0
#define C64_WRITE_LOW   GPIO_BSRR_BR0

#define C64_WRITE       GPIO_IDR_ID0    // R/W on PB0
#define C64_IO1         GPIO_IDR_ID1    // IO1 on PB1
#define C64_IO2         GPIO_IDR_ID2    // IO1 on PB2

#define MENU_BTN        GPIO_IDR_ID7    // Menu button on PB7
#define SPECIAL_BTN     GPIO_IDR_ID8    // Special button on PB8
#define C64_RESET       GPIO_IDR_ID9    // Reset detect on PB9

#define C64_BA          GPIO_IDR_ID10   // BA on PB10
#define C64_ROML        GPIO_IDR_ID11   // ROML on PB11
#define C64_ROMH        GPIO_IDR_ID12   // ROMH on PB12

#define C64_CONTROL_READ()          (GPIOB->IDR)
#define C64_CONTROL_WRITE(state)    GPIOB->BSRR = (state)

static void c64_reset_detect_enable(void)
{
    // Enable PB9 interrupt
    EXTI->PR1 = EXTI_PR1_PR9;
    EXTI->IMR1 |= EXTI_IMR1_IM9;
}

static inline void c64_reset_detect_disable(void)
{
    // Disable PB9 interrupt
    EXTI->IMR1 &= ~EXTI_IMR1_IM9;
}

/******************************************************************************
* C64 IRQ and NMI on PA2 & PA3
* C64 DMA, GAME, and EXROM on PA7 & PA9-PA10
* Status LED on PA15
******************************************************************************/
#define C64_IRQ_HIGH        GPIO_BSRR_BS2
#define C64_IRQ_LOW         GPIO_BSRR_BR2
#define C64_NMI_HIGH        GPIO_BSRR_BS3
#define C64_NMI_LOW         GPIO_BSRR_BR3
#define C64_IRQ_NMI_HIGH    (C64_IRQ_HIGH|C64_NMI_HIGH)
#define C64_IRQ_NMI_LOW     (C64_IRQ_LOW|C64_NMI_LOW)

#define C64_DMA_HIGH        GPIO_BSRR_BS7
#define C64_DMA_LOW         GPIO_BSRR_BR7

#define C64_GAME_HIGH       GPIO_BSRR_BS9
#define C64_GAME_LOW        GPIO_BSRR_BR9
#define C64_EXROM_HIGH      GPIO_BSRR_BS10
#define C64_EXROM_LOW       GPIO_BSRR_BR10

#define STATUS_LED_ON       GPIO_BSRR_BR15
#define STATUS_LED_OFF      GPIO_BSRR_BS15

#define C64_CRT_CONTROL(state) GPIOA->BSRR = (state)

static inline void led_off(void)
{
    C64_CRT_CONTROL(STATUS_LED_OFF);
}

static inline void led_on(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON);
}

static void led_toggle(void)
{
    u32 status_led = STATUS_LED_OFF;
    if (GPIOA->ODR & GPIO_ODR_OD15)
    {
        status_led = STATUS_LED_ON;
    }

    C64_CRT_CONTROL(status_led);
}

/******************************************************************************
* C64 clock input on PA8 (timer 1)
******************************************************************************/

// C64_BUS_HANDLER timing
#define NTSC_PHI2_HIGH      154
#define NTSC_PHI2_INT       (NTSC_PHI2_HIGH - 58)
#define NTSC_PHI2_LOW       236

#define PAL_PHI2_HIGH       164
#define PAL_PHI2_INT        (PAL_PHI2_HIGH - 59)
#define PAL_PHI2_LOW        249

// C64_DMA_BUS_HANDLER timing
#define PHI2_DMA_INT_OFFSET 65
#define PHI2_DMA_OFFSET     76

#define NTSC_PHI2_INT_DMA   (NTSC_PHI2_INT - PHI2_DMA_INT_OFFSET)
#define NTSC_PHI2_HIGH_DMA  (NTSC_PHI2_HIGH - PHI2_DMA_OFFSET)

#define PAL_PHI2_INT_DMA    (PAL_PHI2_INT - PHI2_DMA_INT_OFFSET)
#define PAL_PHI2_HIGH_DMA   (PAL_PHI2_HIGH - PHI2_DMA_OFFSET)

#define NOP()                           \
 /* This dummy read takes 1 cycle */    \
 (void)DWT->CYCCNT

#define WAIT_UNTIL(until)   \
    while (DWT->CYCCNT < until)

#define C64_BUS_HANDLER(name)                                                   \
    C64_BUS_HANDLER_(name##_handler, name##_read_handler, name##_write_handler)

#define C64_BUS_HANDLER_READ(name, read_handler)                                \
    C64_BUS_HANDLER_(name##_handler, read_handler, name##_write_handler)

#define C64_BUS_HANDLER_(handler, read_handler, write_handler)                  \
__attribute__((optimize("O2")))                                                 \
static void handler(void)                                                       \
{                                                                               \
    u32 phi2_high = DWT->COMP0;                                                 \
    COMPILER_BARRIER();                                                         \
    /* We need to clear the interrupt flag early otherwise the next */          \
    /* interrupt may be delayed */                                              \
    TIM1->SR = 0;                                                               \
    /* Use debug cycle counter which is faster to access than timer */          \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    COMPILER_BARRIER();                                                         \
    /* Wait for CPU cycle */                                                    \
    WAIT_UNTIL(phi2_high);                                                      \
    u32 addr = C64_ADDR_READ();                                                 \
    u32 control = C64_CONTROL_READ();                                           \
    COMPILER_BARRIER();                                                         \
    if (control & C64_WRITE)                                                    \
    {                                                                           \
        if (read_handler(control, addr))                                        \
        {                                                                       \
            /* Wait for phi2 to go low */                                       \
            u32 phi2_low = DWT->COMP1;                                          \
            WAIT_UNTIL(phi2_low);                                               \
            /* We releases the bus as fast as possible when phi2 is low */      \
            C64_DATA_INPUT();                                                   \
        }                                                                       \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        u32 data = C64_DATA_READ();                                             \
        write_handler(control, addr, data);                                     \
    }                                                                           \
}

#define C64_REU_BUS_HANDLER(name)                                               \
    C64_BUS_HANDLER_(name##_handler, name##_read_handler, name##_write_handler) \
    C64_REU_BUS_HANDLER_(name##_reu_handler, name##_read_handler,               \
                         name##_write_handler)

#define C64_REU_BUS_HANDLER_(handler, read_handler, write_handler)              \
__attribute__((optimize("O2")))                                                 \
static void handler(void)                                                       \
{                                                                               \
    u32 phi2_high = DWT->COMP0;                                                 \
    COMPILER_BARRIER();                                                         \
    TIM1->SR = 0;                                                               \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    COMPILER_BARRIER();                                                         \
    /* Wait for CPU cycle */                                                    \
    WAIT_UNTIL(phi2_high);                                                      \
    u32 addr = C64_ADDR_READ();                                                 \
    u32 control = C64_CONTROL_READ();                                           \
    COMPILER_BARRIER();                                                         \
    if (control & C64_WRITE)                                                    \
    {                                                                           \
        if (read_handler(control, addr) || reu_read_handler(control, addr))     \
        {                                                                       \
            /* Release bus when phi2 is going low */                            \
            u32 phi2_low = DWT->COMP1;                                          \
            WAIT_UNTIL(phi2_low);                                               \
            C64_DATA_INPUT();                                                   \
        }                                                                       \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        u32 data = C64_DATA_READ();                                             \
        if (!write_handler(control, addr, data))                                \
        {                                                                       \
            reu_write_handler(control, addr, data);                             \
        }                                                                       \
    }                                                                           \
}

#define C64_DMA_BUS_HANDLER(name)                                               \
    C64_DMA_BUS_HANDLER_(name##_dma_handler, name##_dma_bus_handler)

#define C64_DMA_BUS_HANDLER_(handler, dma_handler)                              \
__attribute__((optimize("O2")))                                                 \
static void handler(void)                                                       \
{                                                                               \
    u32 phi2_high = DWT->COMP2;                                                 \
    TIM1->SR = 0;                                                               \
    DWT->CYCCNT = TIM1->CNT;                                                    \
    COMPILER_BARRIER();                                                         \
    /* Wait for CPU cycle */                                                    \
    WAIT_UNTIL(phi2_high);                                                      \
    u32 control = C64_CONTROL_READ();                                           \
    /* Check if we have the bus (no VIC-II access) */                           \
    if (control & C64_BA)                                                       \
    {                                                                           \
        dma_handler();                                                          \
    }                                                                           \
}

#define C64_DMA_DATA_READ()             \
    /* Wait for phi2 to go low */       \
    u32 phi2_low = DWT->COMP1;          \
    COMPILER_BARRIER();                 \
    WAIT_UNTIL(phi2_low);               \
    u32 data = C64_DATA_READ();         \
    COMPILER_BARRIER();                 \
    C64_ADDR_INPUT()

#define C64_DMA_WRITE_END()             \
    /* Wait for phi2 to go low */       \
    u32 phi2_low = DWT->COMP1;          \
    COMPILER_BARRIER();                 \
    WAIT_UNTIL(phi2_low);               \
    C64_CONTROL_WRITE(C64_WRITE_HIGH);  \
    C64_ADDR_INPUT();                   \
    C64_DATA_INPUT()

// C64_VIC_BUS_HANDLER timing
// NTSC
#define NTSC_PHI2_CPU_START     159
#define NTSC_PHI2_WRITE_DELAY   208
#define NTSC_PHI2_CPU_END       236

#define NTSC_PHI2_VIC_START     301
#define NTSC_PHI2_VIC_DELAY     326
#define NTSC_PHI2_VIC_END       369

#define NTSC_WRITE_DELAY()                              \
    /* Wait for data to become ready on the data bus */ \
    WAIT_UNTIL(NTSC_PHI2_WRITE_DELAY)

#define NTSC_VIC_DELAY()                                \
    /* Wait for the control bus to become stable */     \
    WAIT_UNTIL(NTSC_PHI2_VIC_DELAY)

#define NTSC_VIC_DELAY_SHORT()  \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP()

#define NTSC_CPU_VIC_DELAY()    \
    NTSC_VIC_DELAY_SHORT();     \
    NOP()

// PAL
#define PAL_PHI2_CPU_START      168
#define PAL_PHI2_WRITE_DELAY    214
#define PAL_PHI2_CPU_END        246

#define PAL_PHI2_VIC_START      313
#define PAL_PHI2_VIC_DELAY      338
#define PAL_PHI2_VIC_END        384

#define PAL_WRITE_DELAY()                               \
    /* Wait for data to become ready on the data bus */ \
    WAIT_UNTIL(PAL_PHI2_WRITE_DELAY)

#define PAL_VIC_DELAY()                                 \
    /* Wait for the control bus to become stable */     \
    WAIT_UNTIL(PAL_PHI2_VIC_DELAY)

#define PAL_VIC_DELAY_SHORT()   \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP();                      \
    NOP()

#define PAL_CPU_VIC_DELAY()     \
    PAL_VIC_DELAY_SHORT();      \
    NOP()

#define C64_NO_DELAY()

#define C64_EARLY_CPU_VIC_HANDLER(name)                                         \
    /* Note: addr gets overwritten here */                                      \
    addr = name##_early_vic_handler(addr);                                      \
    COMPILER_BARRIER();                                                         \
    NOP();                                                                      \
    NOP();                                                                      \
    NOP();                                                                      \
    NOP()

#define C64_EARLY_VIC_HANDLER(name, timing)                                     \
    C64_EARLY_CPU_VIC_HANDLER(name);                                            \
    timing##_VIC_DELAY_SHORT()

#define C64_VIC_BUS_HANDLER(name)                                               \
    C64_VIC_BUS_HANDLER_(name##_ntsc_handler, NTSC, name)                       \
    C64_VIC_BUS_HANDLER_(name##_pal_handler, PAL, name)

#define C64_VIC_BUS_HANDLER_(handler, timing, name)                             \
    C64_VIC_BUS_HANDLER_EX__(handler,                                           \
                            C64_EARLY_CPU_VIC_HANDLER(name),                    \
                            name##_vic_read_handler, name##_read_handler,       \
                            timing##_WRITE_DELAY(), name##_write_handler,       \
                            C64_EARLY_VIC_HANDLER(name, timing), timing)

#define C64_VIC_BUS_HANDLER_EX(name)                                            \
    C64_VIC_BUS_HANDLER_EX_(name##_ntsc_handler, NTSC, name)                    \
    C64_VIC_BUS_HANDLER_EX_(name##_pal_handler, PAL, name)

#define C64_VIC_BUS_HANDLER_EX_(handler, timing, name)                          \
    C64_VIC_BUS_HANDLER_EX__(handler, timing##_CPU_VIC_DELAY(),                 \
                            name##_vic_read_handler, name##_read_handler,       \
                            name##_early_write_handler(), name##_write_handler, \
                            timing##_VIC_DELAY(), timing)

#define C64_C128_BUS_HANDLER(name)                                              \
    C64_C128_BUS_HANDLER_(name##_ntsc_handler, NTSC, name)                      \
    C64_C128_BUS_HANDLER_(name##_pal_handler, PAL, name)

#define C64_C128_BUS_HANDLER_(handler, timing, name)                            \
    C64_VIC_BUS_HANDLER_EX__(handler, timing##_CPU_VIC_DELAY(),                 \
                            name##_read_handler, name##_read_handler,           \
                            timing##_WRITE_DELAY(), name##_write_handler,       \
                            C64_NO_DELAY(), timing)

// This supports VIC-II reads from the cartridge (i.e. character and sprite data)
// but uses 100% CPU - other interrupts are not served due to the interrupt priority
#define C64_VIC_BUS_HANDLER_EX__(handler, early_cpu_vic_handler,                \
                                 vic_read_handler, read_handler,                \
                                 early_write_handler, write_handler,            \
                                 early_vic_handler, timing)                     \
__attribute__((optimize("O2")))                                                 \
static void handler(void)                                                       \
{                                                                               \
    while (true)                                                                \
    {                                                                           \
        /* Use debug cycle counter which is faster to access than timer */      \
        DWT->CYCCNT = DWT->COMP3 + TIM1->CNT;                                   \
        COMPILER_BARRIER();                                                     \
        /* Wait for CPU cycle */                                                \
        WAIT_UNTIL(timing##_PHI2_CPU_START);                                    \
        u32 addr = C64_ADDR_READ();                                             \
        COMPILER_BARRIER();                                                     \
        u32 control = C64_CONTROL_READ();                                       \
        /* Check if CPU has the bus (no bad line) */                            \
        if ((control & (C64_BA|C64_WRITE)) == (C64_BA|C64_WRITE))               \
        {                                                                       \
            if (read_handler(control, addr))                                    \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                WAIT_UNTIL(timing##_PHI2_CPU_END);                              \
                C64_DATA_INPUT();                                               \
            }                                                                   \
        }                                                                       \
        else if (!(control & C64_WRITE))                                        \
        {                                                                       \
            early_write_handler;                                                \
            u32 data = C64_DATA_READ();                                         \
            write_handler(control, addr, data);                                 \
        }                                                                       \
        /* VIC-II has the bus */                                                \
        else                                                                    \
        {                                                                       \
            /* Wait for the control bus to become stable */                     \
            early_cpu_vic_handler;                                              \
            control = C64_CONTROL_READ();                                       \
            if (vic_read_handler(control, addr))                                \
            {                                                                   \
                /* Release bus when phi2 is going low */                        \
                WAIT_UNTIL(timing##_PHI2_CPU_END);                              \
                C64_DATA_INPUT();                                               \
            }                                                                   \
        }                                                                       \
        if ((control & (C64_RESET|MENU_BTN)) != C64_RESET)                      \
        {                                                                       \
            /* Allow the reset or menu button interrupt handler to run */       \
            break;                                                              \
        }                                                                       \
        /* Wait for VIC-II cycle */                                             \
        WAIT_UNTIL(timing##_PHI2_VIC_START);                                    \
        addr = C64_ADDR_READ();                                                 \
        COMPILER_BARRIER();                                                     \
        /* Ideally, we would always wait until PHI2_VIC_DELAY here which is */  \
        /* required when the VIC-II has the bus, but we need more cycles */     \
        /* in C128 2 MHz mode where data is read from flash */                  \
        early_vic_handler;                                                      \
        control = C64_CONTROL_READ();                                           \
        if (vic_read_handler(control, addr))                                    \
        {                                                                       \
            /* Release bus when phi2 is going high */                           \
            WAIT_UNTIL(timing##_PHI2_VIC_END);                                  \
            C64_DATA_INPUT();                                                   \
        }                                                                       \
        COMPILER_BARRIER();                                                     \
    }                                                                           \
    C64_INTERFACE_DISABLE();                                                    \
}

// TIM1_CC_IRQHandler vector
#define C64_HANDLER (((u32 *)0x00000000)[43])

#define C64_INSTALL_HANDLER(handler)    \
    C64_HANDLER = (u32)(handler)

#define C64_HANDLER_ENABLE()        \
    TIM1->DIER = TIM_DIER_CC3IE

#define C64_DMA_HANDLER_ENABLE()    \
    TIM1->DIER = TIM_DIER_CC4IE

static inline bool c64_is_ntsc(void)
{
    return TIM1->CCR1 < 280;
}

/******************************************************************************
* C64 interface
******************************************************************************/
static inline bool c64_interface_active(void)
{
    return TIM1->DIER != 0;
}

#define C64_INTERFACE_DISABLE()                 \
    /* Capture/Compare 3 interrupt disable */   \
    TIM1->DIER = 0;                             \
    TIM1->SR = 0;                               \
    /* Ensure interrupt flag is cleared */      \
    (void)TIM1->SR

/******************************************************************************
* C64 reset on PA1
******************************************************************************/

static void c64_reset_release(void);
