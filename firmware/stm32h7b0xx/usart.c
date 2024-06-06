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

#define USART_CLK   (140000000UL)
#define BAUD_RATE   (115200)

/******************************************************************************
* Configure USART3 TX/RX PC10 & PC11
******************************************************************************/
static void usart_config(void)
{
    // Enable USART3 clock
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    // Set PC10 & PC11 to low speed
    MODIFY_REG(GPIOC->OSPEEDR, GPIO_OSPEEDR_OSPEED10|GPIO_OSPEEDR_OSPEED11, 0);

    // Set PC10 & PC11 as alternate mode 7 (USART3)
    MODIFY_REG(GPIOC->AFR[1], GPIO_AFRH_AFSEL10|GPIO_AFRH_AFSEL11,
               (7 << GPIO_AFRH_AFSEL10_Pos)|(7 << GPIO_AFRH_AFSEL11_Pos));
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODE10|GPIO_MODER_MODE11,
               GPIO_MODER_MODE10_1|GPIO_MODER_MODE11_1);

    // Configure baud rate
    USART3->BRR = ((USART_CLK + BAUD_RATE/2) / BAUD_RATE);

    // Enable USART3 FIFO mode and TX/RX
    USART3->CR1 = USART_CR1_FIFOEN|USART_CR1_TE|USART_CR1_RE|USART_CR1_UE;
}

/*****************************************************************************/
static inline bool usart_can_putc(void)
{
    return (USART3->ISR & USART_ISR_TXE_TXFNF) != 0;
}

static void usart_putc(char ch)
{
    // Wait for room in the TX FIFO
    while (!usart_can_putc());

    // Send byte
    USART3->TDR = ch;
}

static inline bool usart_gotc(void)
{
    return (USART3->ISR & USART_ISR_RXNE_RXFNE) != 0;
}

static char usart_getc(void)
{
    // Wait for data in the RX FIFO
    while (!usart_gotc());

    return USART3->RDR;
}

static void usart_wait_for_tx(void)
{
    while (!(USART3->ISR & USART_ISR_TC));
}
