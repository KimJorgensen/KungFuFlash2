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
 * Based on CSG8726TechRefDoc-1.0 by Wolfgang Moser
 * and some of the 8726 quirks documented in the VICE emulator source code
 *
 * Emulation is not 100% accurate, run these test programs for current status:
 * https://sourceforge.net/p/vice-emu/code/HEAD/tree/testprogs/REU/
 */

#define REU_BUF         (crt_buf)
#define REU_REG         (crt_ram_buf + 0x100)

#define REU_CMD32       ((u32 *)(REU_REG + 0x01))

#define REU_REG_SHADOW  (REU_REG + 0x20)
#define REU_REG_MASK    (REU_REG + 0x40)

#define REU_BASE_MASK   (0xfffff) // 1MB of RAM

typedef struct
{
    u8 status;              // $df00 Status register
    u8 command;             // $df01 Command register

    u16 c64_base;           // $df02-$df03 C64 base address
    u32 reu_base : 24;      // $df04-$df06 REU base address
    u16 trans_len;          // $df07-$df08 Transfer length

    u8 int_mask;            // $df09 Interrupt mask register
    u8 addr_ctl;            // $df0a Address control register

    u8 unused[0x15];        // $df0b-$df1f Unused (0xff)

    u8 shadow[0x20];        // Shadow registers (REU_REG_SHADOW)

    // KFF specific registers
    u8 mask[0x20];          // Set mask when reading (REU_REG_MASK)

    void (* c64_handler)(void);

    u32 *auto_ptr;
    u8 c64_inc;
    u8 reu_inc;
    u8 temp;

} __attribute__((packed)) REU_REGISTERS;

#define REU         ((REU_REGISTERS *)REU_REG)
#define REU_SHADOW  ((REU_REGISTERS *)REU_REG_SHADOW)

static void reu_handler(void);

static void reu_read_dma_handler(void);
static void reu_write_dma_handler(void);
static void reu_swap_dma_handler(void);
static void reu_swap2_dma_handler(void);
static void reu_verify_dma_handler(void);
static void reu_verify_error_dma_handler(void);

static void (* const reu_handlers[4])(void) =
{
    reu_read_dma_handler,
    reu_write_dma_handler,
    reu_swap_dma_handler,
    reu_verify_dma_handler
};

/******************************************************************************
* C64 bus read callback
******************************************************************************/
FORCE_INLINE bool reu_read_handler(u32 control, u32 addr)
{
    if (!(control & C64_IO2))   // REU registers
    {
        u32 reu_addr = addr & 0x1f;
        u32 data = REU_REG[reu_addr] | REU_REG_MASK[reu_addr];
        C64_DATA_WRITE(data);

        if (!reu_addr) // $df00 Status register
        {
            REU->status = 0x00; // Clear status
            C64_CRT_CONTROL(C64_IRQ_HIGH);
        }
        return true;
    }

    return false;
}

/******************************************************************************
* C64 bus write callback
******************************************************************************/
FORCE_INLINE void reu_write_handler(u32 control, u32 addr, u32 data)
{
if (!(control & C64_IO2))   // REU registers
    {
        u32 reu_addr = addr & 0x1f;
        if (!reu_addr)  // Status register
        {
            return;
        }

        switch (reu_addr)
        {
            case 0x01:  // Command register
            {
                // Setup autoload pointer
                REU->auto_ptr = (u32 *)(&REU_REG[0x01] + (data & 0x20));

                // Execute bit is set and no FF00 trigger
                if ((data & 0x90) == 0x90)
                {
                    // Install the DMA handler for selected transfer type
                    C64_INSTALL_HANDLER(reu_handlers[data & 0x03]);
                    C64_DMA_HANDLER_ENABLE();
                    C64_CRT_CONTROL(C64_DMA_LOW);
                }
            }
            break;

            case 0x02:  // C64 base address LSB
            case 0x04:  // REU base address LSB
            case 0x07:  // Transfer length LSB
            {
                // Emulate the 8726 half-autoload bug
                REU_REG[reu_addr + 1] = REU_REG_SHADOW[reu_addr + 1];
            }
            break;

            case 0x03:  // C64 base address MSB
            case 0x05:  // REU base address MSB
            case 0x08:  // Transfer length MSB
            {
                // Emulate the 8726 half-autoload bug
                REU_REG[reu_addr - 1] = REU_REG_SHADOW[reu_addr - 1];
            }
            break;

            case 0x09:  // Interrupt mask register
            {
                if ((data & 0x60 & REU->status) && (data & 0x80))   // Interrupt
                {
                    REU->status |= 0x80;    // Set interrupt pending
                    C64_CRT_CONTROL(C64_IRQ_LOW);
                }
            }
            break;

            case 0x0a: // Address control register
            {
                REU->c64_inc = ((~data) >> 7) & 0x01;
                REU->reu_inc = ((~data) >> 6) & 0x01;
            }
            break;
        }

        REU_REG[reu_addr] = data;
        REU_REG_SHADOW[reu_addr] = data;
        return;
    }

    if ((addr & 0xffff) == 0xff00 && (REU->command & 0x80))   // FF00 trigger
    {
        // Install the DMA handler for selected transfer type
        C64_INSTALL_HANDLER(reu_handlers[REU->command & 0x03]);
        C64_DMA_HANDLER_ENABLE();
        C64_CRT_CONTROL(C64_DMA_LOW);
    }
}

/******************************************************************************
* C64 bus DMA callback (C64 -> REU)
******************************************************************************/
FORCE_INLINE void reu_read_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);
    REU->c64_base += REU->c64_inc;

    u8 *reu_ptr = REU_BUF + (REU->reu_base & REU_BASE_MASK);
    REU->reu_base = (REU->reu_base & REU_BASE_MASK) + REU->reu_inc;

    if (REU->trans_len != 1)
    {
        REU->trans_len--;

        C64_DMA_DATA_READ();
        *reu_ptr = data;
        return;
    }

    // Copy shadow registers and clear the execute bit and FF00 trigger
    REU_CMD32[0] = (REU->auto_ptr[0] & ~0x80) | 0x10;
    REU_CMD32[1] = REU->auto_ptr[1];

    u8 status = 0x40;   // Set end of block

    u32 crt_control = C64_DMA_HIGH;
    if ((REU->int_mask & 0xc0) == 0xc0) // End of block interrupt
    {
        status |= 0x80; // Set interrupt pending
        crt_control |= C64_IRQ_LOW;
    }
    REU->status |= status;

    C64_DMA_DATA_READ();
    *reu_ptr = data;

    C64_HANDLER_ENABLE();
    C64_INSTALL_HANDLER(REU->c64_handler);  // Restore old C64 bus handler

    // We have to wait until the end of the CPU cycle to release DMA
    C64_CRT_CONTROL(crt_control);
}

/******************************************************************************
* C64 bus DMA callback (REU -> C64)
******************************************************************************/
FORCE_INLINE void reu_write_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);
    C64_CONTROL_WRITE(C64_WRITE_LOW);
    u8 data = REU_BUF[REU->reu_base & REU_BASE_MASK];
    C64_DATA_WRITE(data);

    REU->c64_base += REU->c64_inc;
    REU->reu_base = (REU->reu_base & REU_BASE_MASK) + REU->reu_inc;

    if (REU->trans_len != 1)
    {
        REU->trans_len--;
        C64_DMA_WRITE_END();
        return;
    }

    // Copy shadow registers and clear the execute bit and FF00 trigger
    REU_CMD32[0] = (REU->auto_ptr[0] & ~0x80) | 0x10;
    REU_CMD32[1] = REU->auto_ptr[1];

    u8 status = 0x40;   // Set end of block

    u32 crt_control = C64_DMA_HIGH;
    if ((REU->int_mask & 0xc0) == 0xc0) // End of block interrupt
    {
        status |= 0x80; // Set interrupt pending
        crt_control |= C64_IRQ_LOW;
    }
    REU->status |= status;

    C64_DMA_WRITE_END();

    C64_HANDLER_ENABLE();
    C64_INSTALL_HANDLER(REU->c64_handler);  // Restore old C64 bus handler
    C64_CRT_CONTROL(crt_control);
}

/******************************************************************************
* C64 bus DMA callback (C64 <-> REU) - first part (C64 -> REU)
******************************************************************************/
FORCE_INLINE void reu_swap_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);

    u8 *reu_ptr = REU_BUF + (REU->reu_base & REU_BASE_MASK);
    REU->reu_base = (REU->reu_base & REU_BASE_MASK) + REU->reu_inc;
    REU->temp = *reu_ptr;

    C64_DMA_DATA_READ();
    *reu_ptr = data;

    C64_INSTALL_HANDLER(reu_swap2_dma_handler);
}

/******************************************************************************
* C64 bus DMA callback (C64 <-> REU) - last part (REU -> C64)
******************************************************************************/
FORCE_INLINE void reu_swap2_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);
    REU->c64_base += REU->c64_inc;
    C64_CONTROL_WRITE(C64_WRITE_LOW);
    C64_DATA_WRITE(REU->temp);

    if (REU->trans_len != 1)
    {
        REU->trans_len--;
        C64_INSTALL_HANDLER(reu_swap_dma_handler);
        C64_DMA_WRITE_END();
        return;
    }

    // Copy shadow registers and clear the execute bit and FF00 trigger
    REU_CMD32[0] = (REU->auto_ptr[0] & ~0x80) | 0x10;
    REU_CMD32[1] = REU->auto_ptr[1];

    u8 status = 0x40;   // Set end of block

    u32 crt_control = C64_DMA_HIGH;
    if ((REU->int_mask & 0xc0) == 0xc0) // End of block interrupt
    {
        status |= 0x80; // Set interrupt pending
        crt_control |= C64_IRQ_LOW;
    }
    REU->status |= status;

    C64_DMA_WRITE_END();

    C64_HANDLER_ENABLE();
    C64_INSTALL_HANDLER(REU->c64_handler);  // Restore old C64 bus handler
    C64_CRT_CONTROL(crt_control);
}

/******************************************************************************
* C64 bus DMA callback (C64 == REU) - No verify error
******************************************************************************/
FORCE_INLINE void reu_verify_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);
    REU->c64_base += REU->c64_inc;

    u8 *reu_ptr = REU_BUF + (REU->reu_base & REU_BASE_MASK);
    REU->reu_base = (REU->reu_base & REU_BASE_MASK) + REU->reu_inc;
    u8 reu_data = *reu_ptr;

    if (REU->trans_len != 1)
    {
        REU->trans_len--;

        C64_DMA_DATA_READ();
        if (reu_data != (u8)data)
        {
            // The 8726 uses an extra cycle when an error is detected before it
            // reaches the last byte
            C64_INSTALL_HANDLER(reu_verify_error_dma_handler);
        }
        return;
    }

    // Copy shadow registers and clear the execute bit and FF00 trigger
    REU_CMD32[0] = (REU->auto_ptr[0] & ~0x80) | 0x10;
    REU_CMD32[1] = REU->auto_ptr[1];

    C64_HANDLER_ENABLE();
    C64_INSTALL_HANDLER(REU->c64_handler);  // Restore old C64 bus handler

    C64_DMA_DATA_READ();

    // The 8726 always sets the end of block flag if it reaches the last byte
    u8 status = 0x40;
    if (reu_data != (u8)data)
    {
        status |= 0x20; // Set verify error
    }

    u32 crt_control = C64_DMA_HIGH;
    if ((status & REU->int_mask & 0x60) && (REU->int_mask & 0x80))  // Interrupt
    {
        status |= 0x80; // Set interrupt pending
        crt_control |= C64_IRQ_LOW;
    }
    REU->status |= status;

    C64_CRT_CONTROL(crt_control);
}

/******************************************************************************
* C64 bus DMA callback (C64 == REU) - Verify error
******************************************************************************/
FORCE_INLINE void reu_verify_error_dma_bus_handler(void)
{
    C64_ADDR_WRITE(REU->c64_base);
    u8 reu_data = REU_BUF[REU->reu_base & REU_BASE_MASK];

    bool last_byte = REU->trans_len == 1;
    COMPILER_BARRIER();

    // Copy shadow registers and clear the execute bit and FF00 trigger
    REU_CMD32[0] = (REU->auto_ptr[0] & ~0x80) | 0x10;
    REU_CMD32[1] = REU->auto_ptr[1];

    C64_HANDLER_ENABLE();
    C64_INSTALL_HANDLER(REU->c64_handler);  // Restore old C64 bus handler

    C64_DMA_DATA_READ();

    u8 status = 0x20;   // Set verify error
    if (reu_data == (u8)data && last_byte)
    {
        // The 8726 sets the end of block flag if there is an error on the
        // penultimate byte but not on the last
        status |= 0x40;
    }

    u32 crt_control = C64_DMA_HIGH;
    if ((status & REU->int_mask & 0x60) && (REU->int_mask & 0x80))  // Interrupt
    {
        status |= 0x80; // Set interrupt pending
        crt_control |= C64_IRQ_LOW;
    }
    REU->status |= status;

    C64_CRT_CONTROL(crt_control);
}

static void reu_handler_init(void (c64_handler) (void))
{
    REU->c64_handler = c64_handler;

    // Set registers to default values
    memset(REU_REG, 0, 0x4b);
    memset(REU_REG + 0x4b, 0xff, 0x15);

    REU->command = 0x10;    // No FF00 trigger
    REU->trans_len = 0xffff;
    REU_SHADOW->trans_len = 0xffff;

    // Status register - 0x10 = 1764 (256k) or 1750 (512k) - 0x00 = 1700 (128k)
    REU_REG_MASK[0x00] = 0x10;
    REU_REG_MASK[0x06] = 0xf8;  // REU bank pointer - The 8726 only has 3 bits
    REU_REG_MASK[0x09] = 0x1f;  // Interrupt mask register
    REU_REG_MASK[0x0a] = 0x3f;  // Address control register

    REU->auto_ptr = REU_CMD32;
    REU->c64_inc = 1;
    REU->reu_inc = 1;
}

static void reu_init(void)
{
    reu_handler_init(reu_handler);
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_NONE);
}

// REU handler not combined with other cartridges
C64_BUS_HANDLER(reu)

// REU handlers for the various DMA transfers
C64_DMA_BUS_HANDLER(reu_read)
C64_DMA_BUS_HANDLER(reu_write)
C64_DMA_BUS_HANDLER(reu_swap)
C64_DMA_BUS_HANDLER(reu_swap2)
C64_DMA_BUS_HANDLER(reu_verify)
C64_DMA_BUS_HANDLER(reu_verify_error)
