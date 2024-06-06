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
 *
 ******************************************************************************
 * Based on https://github.com/colosimo/fatfs-stm32
 * 2017, Aurelio Colosimo <aurelio@aureliocolosimo.it>
 * MIT License
 ******************************************************************************
 * and a few bits from https://github.com/rogerclarkmelbourne/Arduino_STM32
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

#include "ff.h"
#include "diskio.h"

#define SDMMC_CLK   (280000000UL)

#define BIT30 (1 << 30)
#define BIT31 ((u32)(1 << 31))

#define RESP_NONE  0
#define RESP_SHORT 1
#define RESP_LONG  2

#define CT_MMC   0x01   // MMC ver 3
#define CT_SD1   0x02   // SD ver 1
#define CT_SD2   0x04   // SD ver 2
#define CT_SDC   (CT_SD1|CT_SD2)    // SD
#define CT_BLOCK 0x08   // Block addressing

#define ACMD(x)     ((x) | 0x80)
#define IS_ACMD(x)  ((x) & 0x80)

#define SDMMC_ICR_CMD_FLAGS (SDMMC_ICR_SDIOITC   | SDMMC_ICR_BUSYD0ENDC |   \
                             SDMMC_ICR_CMDSENTC  | SDMMC_ICR_CMDRENDC   |   \
                             SDMMC_ICR_CTIMEOUTC | SDMMC_ICR_CCRCFAILC)

#define SDMMC_ICR_DATA_FLAGS (SDMMC_ICR_DABORTC   | SDMMC_ICR_DHOLDC    |   \
                              SDMMC_ICR_DBCKENDC  | SDMMC_ICR_DATAENDC  |   \
                              SDMMC_ICR_RXOVERRC  | SDMMC_ICR_TXUNDERRC |   \
                              SDMMC_ICR_DTIMEOUTC | SDMMC_ICR_DCRCFAILC)

#define SDMMC_STA_TRX_ERROR_FLAGS (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT |  \
                                   SDMMC_STA_TXUNDERR | SDMMC_STA_RXOVERR  |  \
                                   SDMMC_STA_DABORT)

static u32 card_rca;
static u8 card_type;
static u8 card_info[36];    // CSD, CID, OCR

static DSTATUS dstatus = STA_NOINIT;


static void sdmmc_init(void)
{
    // Pull-up on SDMMC_D0-D3 & SDMMC_CMD
    MODIFY_REG(GPIOB->PUPDR,
               GPIO_PUPDR_PUPD3|GPIO_PUPDR_PUPD4|
               GPIO_PUPDR_PUPD14|GPIO_PUPDR_PUPD15,
               GPIO_PUPDR_PUPD3_0|GPIO_PUPDR_PUPD4_0|
               GPIO_PUPDR_PUPD14_0|GPIO_PUPDR_PUPD15_0);
    MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD0, GPIO_PUPDR_PUPD0_0);
    MODIFY_REG(GPIOC->PUPDR, GPIO_PUPDR_PUPD1, 0);

    // Set output speed to high speed
    MODIFY_REG(GPIOB->OSPEEDR,
               GPIO_OSPEEDR_OSPEED3|GPIO_OSPEEDR_OSPEED4|
               GPIO_OSPEEDR_OSPEED14|GPIO_OSPEEDR_OSPEED15,
               GPIO_OSPEEDR_OSPEED3_1|GPIO_OSPEEDR_OSPEED4_1|
               GPIO_OSPEEDR_OSPEED14_1|GPIO_OSPEEDR_OSPEED15_1);
    MODIFY_REG(GPIOA->OSPEEDR, GPIO_OSPEEDR_OSPEED0, GPIO_OSPEEDR_OSPEED0_1);
    MODIFY_REG(GPIOC->OSPEEDR, GPIO_OSPEEDR_OSPEED1, GPIO_OSPEEDR_OSPEED1_1);

    // Set PB3-4, PB14-15, PC1, and PA0 to alternate function 9
    // (SDMMC2_D0-D3, SDMMC2_CK & SDMMC2_CMD)
    MODIFY_REG(GPIOB->AFR[0], GPIO_AFRL_AFSEL3|GPIO_AFRL_AFSEL4,
               (9 << GPIO_AFRL_AFSEL3_Pos) | (9 << GPIO_AFRL_AFSEL4_Pos));
    MODIFY_REG(GPIOB->AFR[1], GPIO_AFRH_AFSEL14||GPIO_AFRH_AFSEL15,
               (9 << GPIO_AFRH_AFSEL14_Pos) | (9 << GPIO_AFRH_AFSEL15_Pos));
    MODIFY_REG(GPIOB->MODER,
               GPIO_MODER_MODE3|GPIO_MODER_MODE4|
               GPIO_MODER_MODE14|GPIO_MODER_MODE15,
               GPIO_MODER_MODE3_1|GPIO_MODER_MODE4_1|
               GPIO_MODER_MODE14_1|GPIO_MODER_MODE15_1);

    MODIFY_REG(GPIOA->AFR[0], GPIO_AFRL_AFSEL0, (9 << GPIO_AFRL_AFSEL0_Pos));
    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODE0, GPIO_MODER_MODE0_1);

    MODIFY_REG(GPIOC->AFR[0], GPIO_AFRL_AFSEL1, (9 << GPIO_AFRL_AFSEL1_Pos));
    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODE1, GPIO_MODER_MODE1_1);

    // Set PLL1Q as the SDMMC clock
    MODIFY_REG(RCC->CDCCIPR, RCC_CDCCIPR_SDMMCSEL, 0);

    // Enable SDMMC and reset
    RCC->AHB2ENR |= RCC_AHB2ENR_SDMMC2EN;
    (void)RCC->AHB2ENR;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_SDMMC2RST;
    (void)RCC->AHB2RSTR;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_SDMMC2RST;
    (void)RCC->AHB2RSTR;

    SDMMC2->CLKCR = (SDMMC_CLK / (2 * 400000)); // clk set to 400kHz
    SDMMC2->DTIMER = 24000000;

    SDMMC2->POWER |= SDMMC_POWER_PWRCTRL;
    (void)SDMMC2->POWER;
    delay_us(200);  // Wait for 80 cycles at 400kHz
}

static void sdmmc_deinit(void)
{
    SDMMC2->POWER &= ~SDMMC_POWER_PWRCTRL;
    (void)SDMMC2->POWER;

    RCC->AHB2ENR &= ~RCC_AHB2ENR_SDMMC2EN;
    (void)RCC->AHB2ENR;

    // Set PB3-4, PB14-15, PC1, and PA0 as input
    MODIFY_REG(GPIOB->MODER,
               GPIO_MODER_MODE3|GPIO_MODER_MODE4|
               GPIO_MODER_MODE14|GPIO_MODER_MODE15, 0);
    MODIFY_REG(GPIOB->AFR[0], GPIO_AFRL_AFSEL3|GPIO_AFRL_AFSEL4, 0);
    MODIFY_REG(GPIOB->AFR[1], GPIO_AFRH_AFSEL14||GPIO_AFRH_AFSEL15, 0);

    MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODE0, 0);
    MODIFY_REG(GPIOA->AFR[0], GPIO_AFRL_AFSEL0, 0);

    MODIFY_REG(GPIOC->MODER, GPIO_MODER_MODE1, 0);
    MODIFY_REG(GPIOC->AFR[0], GPIO_AFRL_AFSEL1, 0);

    // No pull-up, pull-down on PB3-4, PB14-15, PC1, and PA0
    MODIFY_REG(GPIOB->PUPDR,
               GPIO_PUPDR_PUPD3|GPIO_PUPDR_PUPD4|
               GPIO_PUPDR_PUPD14|GPIO_PUPDR_PUPD15, 0);
    MODIFY_REG(GPIOA->PUPDR, GPIO_PUPDR_PUPD0, 0);
    MODIFY_REG(GPIOC->PUPDR, GPIO_PUPDR_PUPD1, 0);
}

static void byte_swap(u8 *dest, u32 src)
{
    u32 *dest32 = (u32 *)dest;
    *dest32 = __REV(src);
}

static bool sdmmc_cmd_send(u8 idx, u32 arg, int resp_type, u32 *buf)
{
    if (IS_ACMD(idx)) // ACMD class
    {
        if (!sdmmc_cmd_send(55, card_rca, RESP_SHORT, buf) ||
            !(buf[0] & 0x00000020))
        {
            return false;
        }
    }

    idx &= SDMMC_CMD_CMDINDEX;

    u32 cmd = SDMMC_CMD_CPSMEN | idx;
    if (resp_type == RESP_SHORT)
    {
        cmd |= SDMMC_CMD_WAITRESP_0;
    }
    else if (resp_type == RESP_LONG)
    {
        cmd |= SDMMC_CMD_WAITRESP_0|SDMMC_CMD_WAITRESP_1;
    }

    SDMMC2->ICR = SDMMC_ICR_CMD_FLAGS;
    (void)SDMMC2->ICR;
    SDMMC2->ARG = arg;
    SDMMC2->CMD = cmd;

    // Wait for command transfer to finish
    while (SDMMC2->STA & SDMMC_STA_CPSMACT);

    if (resp_type == RESP_NONE)
    {
        return SDMMC2->STA & SDMMC_STA_CMDSENT ? true : false;
    }
    else
    {
        u32 sta;
        // Wait for response
        do
        {
            sta = SDMMC2->STA;
        }
        while (!(sta & (SDMMC_STA_CMDREND|SDMMC_STA_CTIMEOUT|SDMMC_STA_CCRCFAIL)));

        // Check if timeout
        if (sta & SDMMC_STA_CTIMEOUT)
        {
            // Don't get spammed if no card is inserted or if card is removed
            if (!(dstatus & STA_NOINIT) && idx != 13)
            {
                err("%s timeout idx=%u arg=%08x", __func__, idx, arg);
            }
            return false;
        }

        // Check if crc error
        if (sta & SDMMC_STA_CCRCFAIL)
        {
            // Ignore CRC error for these commands
            if (idx != 1 && idx != 12 && idx != 41)
            {
                err("%s crcfail idx=%u arg=%08x", __func__, idx, arg);
                return false;
            }
        }
    }

    buf[0] = SDMMC2->RESP1;
    if (resp_type == RESP_LONG)
    {
        buf[1] = SDMMC2->RESP2;
        buf[2] = SDMMC2->RESP3;
        buf[3] = SDMMC2->RESP4;
    }

    return true;
}

static bool sdmmc_check_ready(void)
{
    u32 resp;
    int i = 0;

    timer_start_ms(250);
    while (!timer_elapsed() || ++i < 2)
    {
        if (sdmmc_cmd_send(13, card_rca, RESP_SHORT, &resp)
            && (resp & 0x0100))
        {
            return true;
        }
    }

    err("sdmmc timeout");
    return false;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    u32 resp[4];
    u8 cmd;
    u8 timeouts;
    int i;

    card_rca = 0;
    dstatus = STA_NOINIT;
    sdmmc_init();

    // Note: No support for card detect (SDMMC_CD)

    if (!sdmmc_cmd_send(0, 0, RESP_NONE, NULL))
    {
        err("could not reset card");
        goto fail;
    }

    timeouts = 4;
    timer_start_ms(250);

    if (sdmmc_cmd_send(8, 0x1AA, RESP_SHORT, resp) && ((resp[0] & 0xfff) == 0x1aa))
    {
        // sdc v2
        card_type = 0;
        do
        {
            if (sdmmc_cmd_send(ACMD(41), 0x40ff8000, RESP_SHORT, resp) &&
                (resp[0] & BIT31))
            {
                card_type = (resp[0] & BIT30) ? CT_SD2 | CT_BLOCK : CT_SD2;
                dbg("card type: SD2");
                break;
            }
        }
        while (!timer_elapsed() || --timeouts);

        if (!card_type)
        {
            err("could not read card type");
            goto fail;
        }
    }
    else
    {
        // sdc v1 or mmc
        if (sdmmc_cmd_send(ACMD(41), 0x00ff8000, RESP_SHORT, resp))
        {
            // ACMD41 is accepted -> sdc v1
            card_type = CT_SD1;
            cmd = ACMD(41);
        }
        else
        {
            // ACMD41 is rejected -> mmc
            card_type = CT_MMC;
            cmd = 1;
        }

        while (true)
        {
            if (sdmmc_cmd_send(cmd, 0x00FF8000, RESP_SHORT, resp) &&
                (resp[0] & BIT31))
            {
                break;
            }

            if (timer_elapsed() && --timeouts)
            {
                dbg("cmd %u failed", cmd);
                goto fail;
            }
        }
    }

    byte_swap(&card_info[32], resp[0]);
    dbg("card OCR: %08x", ((u32*)card_info)[8]);

    // card state 'ready'
    if (!sdmmc_cmd_send(2, 0, RESP_LONG, resp)) // enter ident state
    {
        goto fail;
    }

    for (i = 0; i < 4; i++)
    {
        byte_swap(&card_info[16 + i * 4], resp[i]);
    }

    // card state 'ident'
    if (card_type & CT_SDC)
    {
        if (!sdmmc_cmd_send(3, 0, RESP_SHORT, resp))
        {
            goto fail;
        }

        card_rca = resp[0] & 0xFFFF0000;
    }
    else
    {
        if (!sdmmc_cmd_send(3, 1 << 16, RESP_SHORT, resp))
        {
            goto fail;
        }

        card_rca = 1 << 16;
    }

    // card state 'standby'
    if (!sdmmc_cmd_send(9, card_rca, RESP_LONG, resp))
    {
        goto fail;
    }

    for (i = 0; i < 4; i++)
    {
        byte_swap(&card_info[i * 4], resp[i]);
    }

    if (!sdmmc_cmd_send(7, card_rca, RESP_SHORT, resp))
    {
        goto fail;
    }

    // card state 'tran'
    if (!(card_type & CT_BLOCK))
    {
        if (!sdmmc_cmd_send(16, 512, RESP_SHORT, resp) || (resp[0] & 0xfdf90000))
        {
            goto fail;
        }
    }

    u32 clkcr = SDMMC2->CLKCR;
    if (card_type & CT_SDC)
    {
        // Set wide bus
        if (!sdmmc_cmd_send(ACMD(6), 2, RESP_SHORT, resp) || (resp[0] & 0xfdf90000))
        {
            goto fail;
        }

        clkcr = (clkcr & ~SDMMC_CLKCR_WIDBUS) | SDMMC_CLKCR_WIDBUS_0;
    }

    // Increase clock frequency to 14 MHz
    SDMMC2->CLKCR = (clkcr & ~SDMMC_CLKCR_CLKDIV) | (SDMMC_CLK / (2 * 14000000));
    (void)SDMMC2->CLKCR;
    delay_us(10);   // Wait for more than 80 cycles at 14 MHz

    dstatus &= ~STA_NOINIT;
    return RES_OK;

fail:
    dstatus = STA_NOINIT;
    sdmmc_deinit();
    return RES_ERROR;
}

DSTATUS disk_status(BYTE pdrv)
{
    return dstatus;
}

static bool disk_read_imp(BYTE* buf, DWORD sector, UINT count)
{
    SDMMC2->ICR = SDMMC_ICR_DATA_FLAGS;
    SDMMC2->DLEN = 512 * count;
    SDMMC2->DCTRL = SDMMC_DCTRL_DBLOCKSIZE_0|SDMMC_DCTRL_DBLOCKSIZE_3|
                    SDMMC_DCTRL_DTDIR|SDMMC_DCTRL_DTEN;
    (void)SDMMC2->DCTRL;

    // Send command to start data transfer
    u8 cmd = (count > 1) ? 18 : 17;
    u32 resp;
    if (!sdmmc_cmd_send(cmd, sector, RESP_SHORT, &resp) || (resp & 0xc0580000))
    {
        return false;
    }

    u32 *buf32 = (u32 *)buf;
    const u32 *buf32_end = (u32 *)(buf + 512 * count);

    u32 sta;
    do
    {
        sta = SDMMC2->STA;
        if (!(sta & SDMMC_STA_RXFIFOE) && buf32 < buf32_end)
        {
            *buf32++ = SDMMC2->FIFO;
        }
    }
    while (!(sta & (SDMMC_STA_DATAEND|SDMMC_STA_TRX_ERROR_FLAGS)));

    // Read data still in FIFO
	while (!(SDMMC2->STA & SDMMC_STA_RXFIFOE) && buf32 < buf32_end)
    {
        *buf32++ = SDMMC2->FIFO;
	}

    if (sta & SDMMC_STA_TRX_ERROR_FLAGS)
    {
        wrn("%s SDMMC_STA: %08x", __func__, sta);
    }

    // Stop multi block transfer
    if (cmd == 18)
    {
        sdmmc_cmd_send(12, 0, RESP_SHORT, &resp);
    }

    return !(sta & SDMMC_STA_TRX_ERROR_FLAGS);
}

DRESULT disk_read(BYTE pdrv, BYTE* buf, DWORD sector, UINT count)
{
    led_toggle();

    if (count < 1 || count > 128)
    {
        return RES_PARERR;
    }

    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    if (!(card_type & CT_BLOCK))
    {
        sector *= 512;
    }

    bool result = false;
    for (u32 retry=0; retry<10 && !result; retry++)
    {
        if (!sdmmc_check_ready())
        {
            return RES_ERROR;
        }

        result = disk_read_imp(buf, sector, count);
    }

    return result ? RES_OK : RES_ERROR;
}

static bool disk_write_imp(const BYTE* buf, DWORD sector, UINT count)
{
    u32 resp;
    u8 cmd;

    if (count == 1) // Single block write
    {
        cmd = 24;
    }
    else // Multiple block write
    {
        cmd = (card_type & CT_SDC) ? ACMD(23) : 23;
        if (!sdmmc_cmd_send(cmd, count, RESP_SHORT, &resp) || (resp & 0xC0580000))
        {
            return false;
        }

        cmd = 25;
    }

    SDMMC2->ICR = SDMMC_ICR_DATA_FLAGS;
    SDMMC2->DLEN = 512 * count;

    const u32 *buf32 = (const u32 *)buf;
    u32 first_word = *buf32++;
    __DSB();

    if (!sdmmc_cmd_send(cmd, sector, RESP_SHORT, &resp) || (resp & 0xC0580000))
    {
        err("%s %u", __func__, __LINE__);
        return false;
    }

    // Note: Will not work while the C64 interrupt handler is enabled
    __disable_irq();
    SDMMC2->DCTRL = SDMMC_DCTRL_DBLOCKSIZE_0|SDMMC_DCTRL_DBLOCKSIZE_3|
                    SDMMC_DCTRL_DTEN;

    // Send the first 8 words to avoid TX FIFO underrun
    SDMMC2->FIFO = first_word;
    for (u32 i=0; i<7; i++)
    {
        SDMMC2->FIFO = *buf32++;
    }

    const u32 *buf32_end = (u32 *)(buf + 512 * count);

    u32 sta;
    do
    {
        sta = SDMMC2->STA;
        if (!(sta & SDMMC_STA_TXFIFOF) && buf32 < buf32_end)
        {
            SDMMC2->FIFO = *buf32++;
        }
    }
    while (!(sta & (SDMMC_STA_DATAEND|SDMMC_STA_TRX_ERROR_FLAGS)));
    __enable_irq();

    if (sta & SDMMC_STA_TRX_ERROR_FLAGS)
    {
        wrn("%s SDMMC_STA: %08x", __func__, sta);
    }

    // Stop multi block transfer
    if (cmd == 25)
    {
        sdmmc_cmd_send(12, 0, RESP_SHORT, &resp);
    }

    return !(sta & SDMMC_STA_TRX_ERROR_FLAGS);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buf, DWORD sector, UINT count)
{
    led_toggle();

    if (count < 1 || count > 128)
    {
        return RES_PARERR;
    }

    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    // Note: No check of Write Protect Pin

    if (!(card_type & CT_BLOCK))
    {
        sector *= 512;
    }

    if (c64_interface_active())
    {
        err("%s C64 interface active", __func__);
        return RES_ERROR;
    }

    bool result = false;
    for (u32 retry=0; retry<3 && !result; retry++)
    {
        if (!sdmmc_check_ready())
        {
            return RES_ERROR;
        }

        result = disk_write_imp(buf, sector, count);
    }

    return result ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (dstatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    // Complete pending write process
    if (cmd == CTRL_SYNC)
    {
        return RES_OK;
    }

    return RES_ERROR;
}
