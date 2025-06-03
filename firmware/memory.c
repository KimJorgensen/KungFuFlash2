/*
 * Copyright (c) 2019-2025 Kim Jørgensen
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

#include "memory.h"

/******************************************************************************
* Menu signature
******************************************************************************/
static inline void set_menu_signature(void)
{
    memcpy(MEMU_SIGNATURE_BUF, MENU_RAM_SIGNATURE, sizeof(MENU_RAM_SIGNATURE));
}

static inline bool menu_signature(void)
{
    return memcmp(MEMU_SIGNATURE_BUF, MENU_RAM_SIGNATURE,
                  sizeof(MENU_RAM_SIGNATURE)) == 0;
}

static inline void invalidate_menu_signature(void)
{
    *MEMU_SIGNATURE_BUF = 0;
}

/******************************************************************************
* CRT image buffer
******************************************************************************/
static void crt_buf_valid(u8 banks)
{
    memcpy(crt_buf_header.signature, CRT_BUF_SIGNATURE,
           sizeof(CRT_BUF_SIGNATURE));

    crt_buf_header.banks = banks;
    crt_buf_header.updated = false;
}

static inline bool crt_buf_is_valid(void)
{
    return memcmp(crt_buf_header.signature, CRT_BUF_SIGNATURE,
                  sizeof(CRT_BUF_SIGNATURE)) == 0;
}

static bool crt_buf_is_updated(void)
{
    return crt_buf_is_valid() && crt_buf_header.updated == true;
}

static inline void crt_buf_invalidate(void)
{
    crt_buf_header.signature[0] = 0;
}

static bool crt_bank_empty(u8 *buf, u16 size)
{
    u32 *buf32 = (u32 *)buf;
    for (u16 i=0; i<size/4; i++)
    {
        if (buf32[i] != 0xffffffff)
        {
            return false;
        }
    }

    return true;
}
