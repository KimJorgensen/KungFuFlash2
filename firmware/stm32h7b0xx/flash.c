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

#define FLASH_KEYR_KEY1     0x45670123
#define FLASH_KEYR_KEY2     0xCDEF89AB

static inline void flash_unlock(void)
{
    FLASH->KEYR1 = FLASH_KEYR_KEY1;
    FLASH->KEYR1 = FLASH_KEYR_KEY2;
}

static inline void flash_lock(void)
{
    FLASH->CR1 = FLASH_CR_LOCK;
}

static void flash_sector_erase(u32 sector)
{
    // Request sector erase
    FLASH->CR1 = (sector << FLASH_CR_SNB_Pos)|FLASH_CR_SER;

    // Start the erase operation
    FLASH->CR1 |= FLASH_CR_START;

    // Wait for the operation to complete
    while (FLASH->SR1 & FLASH_SR_QW);
}

static void flash_program(void *dest, const void *src, size_t bytes)
{
    volatile u64 *dest_ptr = (u64 *)dest;
    const u64 *src_ptr = (const u64 *)src;

    // Activate flash programming
    MODIFY_REG(FLASH->CR1, FLASH_CR_SER, FLASH_CR_PG);

	for (size_t i=0; i<bytes; i+=16)
    {
		*dest_ptr++ = *src_ptr++;
        *dest_ptr++ = *src_ptr++;

        // Wait for the operation to complete
        while (FLASH->SR1 & FLASH_SR_QW);
	}

    // Deactivate flash programming
    MODIFY_REG(FLASH->CR1, FLASH_CR_PG, 0);
}

static void flash_sector_program(u32 sector, void *dest, const void *src, size_t bytes)
{
    // Prevent anything executing from flash
    __disable_irq();

    flash_unlock();
    flash_sector_erase(sector);
    flash_program(dest, src, bytes);
    flash_lock();

    __enable_irq();
}
