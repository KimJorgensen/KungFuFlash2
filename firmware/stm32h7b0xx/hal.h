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

// Main PLL = (source_clock / M) * N  / P
// HSE = 25 MHz external oscillator
// fCLK = (25 MHz / 5) * 112  / 2 = 280 MHz
#define PLL_M1  5
#define PLL_N1 	112
#define PLL_P1 	2
#define PLL_Q1 	2
#define PLL_R1 	2

// PLL3 used for USB
// fPLL3Q = (25 MHz / 5) * 96  / 10 = 48 MHz
#define PLL_M3  5
#define PLL_N3 	96
#define PLL_P3 	2
#define PLL_Q3 	10
#define PLL_R3 	2

static void usbclk_config(void);

static bool filesystem_unmount(void);
static void system_restart(void);
static void restart_to_menu(void);

static void delay_us(u32 us);
static void delay_ms(u32 ms);

static void timer_start_us(u32 us);
static void timer_start_ms(u32 ms);
static void timer_reset(void);
static bool timer_elapsed(void);
