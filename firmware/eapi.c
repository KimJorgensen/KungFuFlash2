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

static void eapi_handle_write_flash(u16 addr, u8 value)
{
    u8 *dest = crt_ptr + (addr & 0x3fff);
    *dest &= value;

    u8 result = REPLY_EAPI_OK;
    if (*dest != value)
    {
        wrn("Flash write failed at $%04x (%x)", addr, crt_ptr);
        result = REPLY_WRITE_ERROR;
    }

    crt_buf_header.updated = true;
    ef3_send_reply(result);
}

static void eapi_handle_erase_sector(u8 bank, u16 addr)
{
    if (bank > 56 || (bank % 8))
    {
        wrn("Got invalid sector to erase: bank %u", bank);
        ef3_send_reply(REPLY_WRITE_ERROR);
        return;
    }

    u16 offset = (addr & 0xff00) == 0x8000 ? 0 : 8*1024;

    // Erase 64k sector
    for (u8 i=0; i<8; i++)
    {
        memset(crt_banks[bank + i] + offset, 0xff, 8*1024);
    }

    crt_buf_header.updated = true;
    ef3_send_reply(REPLY_EAPI_OK);
}

static void eapi_loop(void)
{
    u16 addr;
    u8 value;

    while (true)
    {
        u8 command = ef3_receive_command();
        switch (command)
        {
            case CMD_EAPI_NONE:
            break;

            case CMD_EAPI_INIT:
            {
                dbg("Got EAPI_INIT command");
                ef3_send_reply(REPLY_EAPI_OK);
            }
            break;

            case CMD_WRITE_FLASH:
            {
                ef3_receive_data(&addr, 2);
                value = ef3_receive_byte();
                eapi_handle_write_flash(addr, value);
            }
            break;

            case CMD_ERASE_SECTOR:
            {
                ef3_receive_data(&addr, 2);
                value = ef3_receive_byte();
                dbg("Got ERASE_SECTOR command (%u:$%04x)", value, addr);
                eapi_handle_erase_sector(value, addr);
            }
            break;

            default:
            {
                wrn("Got unknown EAPI command: %x", command);
                ef3_send_reply(REPLY_EAPI_OK);
            }
            break;
        }
    }
}
