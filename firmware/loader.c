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

#define CFG_FILENAME "/.KFF2.cfg"

#define CRT_C64_SIGNATURE  "C64 CARTRIDGE   "
#define CRT_C128_SIGNATURE "C128 CARTRIDGE  "
#define CRT_CHIP_SIGNATURE "CHIP"
#define CRT_VERSION_1_0 0x100
#define CRT_VERSION_2_0 0x200

#define EAPI_OFFSET 0x3800
#define EAPI_SIZE   0x300

static u16 prg_load_file(FIL *file)
{
    u16 len = file_read(file, dat_buf, sizeof(dat_buf));
    if (!prg_size_valid(len))
    {
        wrn("Unsupported PRG size: %u", len);
        return 0;
    }

    return len;
}

static u16 p00_load_file(FIL *file, char *name)
{
    P00_HEADER header;
    u16 len = file_read(file, &header, sizeof(P00_HEADER));

    if (len != sizeof(P00_HEADER) ||
        memcmp("C64File", header.signature, sizeof(header.signature)) != 0)
    {
        wrn("Unsupported P00 header");
        return 0;
    }

    len = prg_load_file(file);
    if (!len)
    {
        return 0;
    }

    memcpy(name, header.filename, sizeof(header.filename));
    name[16] = 0; // Null terminate
    return len;
}

static u16 rom_load_file(FIL *file)
{
    memset(crt_buf, 0xff, 64*1024);

    u16 len = file_read(file, crt_buf, 64*1024);
    return len;
}

static void rom_mirror(u16 len)
{
    // Mirror the first 4k
    if (len <= 4*1024)
    {
        memcpy(&crt_buf[4*1024], crt_buf, 4*1024);
    }

    // Mirror the first 8k
    if (len <= 8*1024)
    {
        memcpy(&crt_buf[8*1024], crt_buf, 8*1024);
    }
}

static u16 txt_load_file(FIL *file)
{
    memset(KFF_BUF, 0x00, sizeof(KFF_BUF));

    u16 len = file_read(file, &KFF_BUF[DIR_NAME_LENGTH+2],
                        sizeof(KFF_BUF) - (DIR_NAME_LENGTH+3));
    return len;
}

static bool crt_load_header(FIL *file, CRT_HEADER *header)
{
    u32 len = file_read(file, header, sizeof(CRT_HEADER));

    if (len != sizeof(CRT_HEADER))
    {
        wrn("Unsupported CRT header");
        return false;
    }

    u16 crt_type_flag = 0;
    if (memcmp(CRT_C128_SIGNATURE, header->signature,
               sizeof(header->signature)) == 0)
    {
        crt_type_flag = CRT_C128_CARTRIDGE;
    }
    else if (memcmp(CRT_C64_SIGNATURE, header->signature,
                    sizeof(header->signature)) != 0)
    {
        wrn("Unsupported CRT signature");
        return false;
    }

    header->header_length = __REV(header->header_length);
    header->version = __REV16(header->version);
    header->cartridge_type = __REV16(header->cartridge_type) | crt_type_flag;

    if (header->header_length != sizeof(CRT_HEADER))
    {
        if (header->header_length != 0x20)
        {
            wrn("Unsupported CRT header length: %u", header->header_length);
            return false;
        }
        else
        {
            log("Ignoring non-standard CRT header length: %u",
                header->header_length);
        }
    }

    if (header->version < CRT_VERSION_1_0 || header->version > CRT_VERSION_2_0)
    {
        wrn("Unsupported CRT version: %x", header->version);
        return false;
    }

    return true;
}

static bool crt_write_header(FIL *file, u16 type, u8 exrom, u8 game, const char *name)
{
    CRT_HEADER header;
    memcpy(header.signature, CRT_C64_SIGNATURE, sizeof(header.signature));
    header.header_length = __REV(sizeof(CRT_HEADER));
    header.version = __REV16(CRT_VERSION_1_0);
    header.cartridge_type = __REV16(type);
    header.exrom = exrom;
    header.game = game;
    header.hardware_revision = 0;
    memset(header.reserved, 0, sizeof(header.reserved));

    for (u8 i=0; i<sizeof(header.cartridge_name); i++)
    {
        char c = *name;
        if (c)
        {
            name++;
        }

        header.cartridge_name[i] = c;
    }

    u32 len = file_write(file, &header, sizeof(CRT_HEADER));
    return len == sizeof(CRT_HEADER);
}

static bool crt_load_chip_header(FIL *file, CRT_CHIP_HEADER *header)
{
    u32 len = file_read(file, header, sizeof(CRT_CHIP_HEADER));

    if (len != sizeof(CRT_CHIP_HEADER) ||
        memcmp(CRT_CHIP_SIGNATURE, header->signature, sizeof(header->signature)) != 0)
    {
        return false;
    }

    header->packet_length = __REV(header->packet_length);
    header->chip_type = __REV16(header->chip_type);
    header->bank = __REV16(header->bank);
    header->start_address = __REV16(header->start_address);
    header->image_size = __REV16(header->image_size);

    // Note: packet length > image size + chip header in "Expert Cartridge" CRT file
    if (header->packet_length < (header->image_size + sizeof(CRT_CHIP_HEADER)))
    {
        return false;
    }

    return true;
}

static bool crt_write_chip_header(FIL *file, u8 type, u8 bank, u16 address, u16 size)
{
    CRT_CHIP_HEADER header;
    memcpy(header.signature, CRT_CHIP_SIGNATURE, sizeof(header.signature));
    header.packet_length = __REV(size + sizeof(CRT_CHIP_HEADER));
    header.chip_type = __REV16(type);
    header.bank = __REV16(bank);
    header.start_address = __REV16(address);
    header.image_size = __REV16(size);

    u32 len = file_write(file, &header, sizeof(CRT_CHIP_HEADER));
    return len == sizeof(CRT_CHIP_HEADER);
}

static s32 crt_get_offset(CRT_CHIP_HEADER *header, u16 cartridge_type)
{
    s32 offset = -1;

    // ROML bank (and ROMH for >8k images)
    if (header->start_address == 0x8000 && header->image_size <= 16*1024)
    {
        // Suport ROML only cartridges with more than 64 banks
        if (header->image_size <= 8*1024 &&
            (cartridge_type == CRT_FUN_PLAY_POWER_PLAY ||
             cartridge_type == CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA))
        {
            bool odd_bank = header->bank & 1;
            header->bank >>= 1;
            offset = header->bank * 16*1024;

            // Use ROMH bank location for odd banks
            if (odd_bank)
            {
                offset += 8*1024;
            }
        }
        else
        {
            if (cartridge_type & CRT_C128_CARTRIDGE)
            {
                header->bank *= 2;
            }
            offset = header->bank * 16*1024;
        }
    }
    // ROMH bank
    else if ((header->start_address == 0xa000 || header->start_address == 0xe000) &&
              header->image_size <= 8*1024)
    {
        offset = header->bank * 16*1024 + 8*1024;
    }
    // ROMH bank (C128)
    else if (header->start_address == 0xc000 && header->image_size <= 16*1024)
    {
        header->bank = (header->bank * 2) + 1;
        offset = header->bank * 16*1024;
    }
    // ROMH bank (4k Ultimax)
    else if (header->start_address == 0xf000 && header->image_size <= 4*1024)
    {
        offset = header->bank * 16*1024 + 8*1024;
    }

    return offset;
}

static u8 crt_load_file(FIL *crt_file, u16 cartridge_type)
{
    memset(crt_buf, 0xff, sizeof(crt_buf));
    u8 banks_in_use = 0;

    while (!f_eof(crt_file))
    {
        CRT_CHIP_HEADER header;
        if (!crt_load_chip_header(crt_file, &header))
        {
            err("Failed to read CRT chip header");
            return 0;
        }

        s32 offset = crt_get_offset(&header, cartridge_type);
        if (offset == -1)
        {
            wrn("Unsupported CRT chip bank %u at $%x. Size %u",
                header.bank, header.start_address, header.image_size);
            return 0;
        }

        if (header.bank >= 64)
        {
            wrn("No room for CRT chip bank %u at $%x",
                header.bank, header.start_address);
            continue;   // Skip image
        }

        if (banks_in_use < (header.bank + 1))
        {
            banks_in_use = header.bank + 1;
        }

        u8 *read_buf = crt_buf + offset;
        if (file_read(crt_file, read_buf, header.image_size) != header.image_size)
        {
            err("Failed to read CRT chip image. Bank %u at $%x",
                header.bank, header.start_address);
            return 0;
        }

        // Mirror 4k image
        if (header.image_size <= 4*1024)
        {
            memcpy(&read_buf[4*1024], read_buf, 4*1024);
        }
    }

    return banks_in_use;
}

static void crt_install_eapi(u16 cartridge_type)
{
    if (cartridge_type == CRT_EASYFLASH &&
        memcmp("eapi", crt_buf + EAPI_OFFSET, 4) == 0)
    {
        convert_to_ascii(scratch_buf, crt_buf + EAPI_OFFSET + 4, 16);
        dbg("EAPI detected: %s", scratch_buf);

        // Replace EAPI with the one for Kung Fu Flash
        memcpy(crt_buf + EAPI_OFFSET, CRT_LAUNCHER + EAPI_OFFSET, EAPI_SIZE);
    }
}

static bool crt_write_chip(FIL *file, u8 bank, u16 address, u16 size, void *buf)
{
    if (!crt_write_chip_header(file, CRT_CHIP_FLASH, bank, address, size))
    {
        return false;
    }

    return file_write(file, buf, size) == size;
}

static u8 crt_write_file(FIL *crt_file)
{
    u8 banks_in_use = 0;

    const u16 chip_size = 8*1024;
    for (u8 bank=0; bank<64; bank++)
    {
        u8 *buf = crt_banks[bank];

        if (!crt_bank_empty(buf, chip_size))
        {
            if (!crt_write_chip(crt_file, bank, 0x8000, chip_size, buf))
            {
                return 0;
            }
            banks_in_use = bank + 1;
        }

        if (!bank || !crt_bank_empty(buf + chip_size, chip_size))
        {
            if (!crt_write_chip(crt_file, bank, 0xa000, chip_size, buf + chip_size))
            {
                return 0;
            }
            banks_in_use = bank + 1;
        }
    }

    return banks_in_use;
}

static bool upd_load(FIL *file, char *firmware_name)
{
    crt_buf_invalidate();

    u32 len = file_read(file, crt_buf, UPD_FILE_SIZE);
    if (len == UPD_FILE_SIZE)
    {
        const u8 *firmware_ver = &crt_buf[UPD_FILE_VER];
        convert_to_ascii(firmware_name, firmware_ver, FW_NAME_SIZE);

        dbg("UPD file loaded: %s", firmware_name);
        if (memcmp(firmware_name, "Kung Fu Flash v2", 16) == 0)
        {
            return true;
        }
    }

    return false;
}

static void upd_program(void)
{
    for (u32 sector=0; sector<16; sector++)
    {
        led_toggle();
        u32 offset = 8*1024 * sector;
        flash_sector_program(sector, (u8 *)FLASH_BASE + offset,
                             crt_buf + offset, 8*1024);
    }
}

static bool mount_sd_card(void)
{
    if (!filesystem_mount())
    {
        return false;
    }

    log("SD Card successfully mounted");
    return dir_change("/");
}

static bool load_cfg(void)
{
    bool result = true;
    FIL file;
    if (!file_open(&file, CFG_FILENAME, FA_READ) ||
        file_read(&file, &cfg_file, sizeof(cfg_file)) != sizeof(cfg_file) ||
        memcmp(CFG_SIGNATURE, cfg_file.signature, sizeof(cfg_file.signature)) != 0)
    {
        wrn("%s file not found or invalid", CFG_FILENAME);
        memset(&cfg_file, 0, sizeof(cfg_file));
        memcpy(cfg_file.signature, CFG_SIGNATURE, sizeof(cfg_file.signature));
        result = false;
    }

    file_close(&file);
    return result;
}

static bool auto_boot(void)
{
    bool result = false;

    load_cfg();
    if (menu_signature() || menu_button_pressed())
    {
        invalidate_menu_signature();

        u32 i = 0;
        while (menu_button_pressed())
        {
            // Menu button long press will start diagnostic
            if (++i > 100)
            {
                cfg_file.boot_type = CFG_DIAG;
                result = true;
                break;
            }
        }
    }
    else
    {
        result = true;
    }

    if (cfg_file.boot_type != CFG_DIAG)
    {
        menu_button_enable();
    }

    return result;
}

static bool save_cfg(void)
{
    dbg("Saving %s file", CFG_FILENAME);

    FIL file;
    if (!file_open(&file, CFG_FILENAME, FA_WRITE|FA_CREATE_ALWAYS))
    {
        wrn("Could not open %s for writing", CFG_FILENAME);
        return false;
    }

    bool file_saved = false;
    if (file_write(&file, &cfg_file, sizeof(cfg_file)) == sizeof(cfg_file))
    {
        file_saved = true;
    }

    file_close(&file);
    return file_saved;
}

static inline bool persist_basic_selection(void)
{
    return (cfg_file.flags & CFG_FLAG_NO_PERSIST) == 0;
}

static inline bool autostart_d64(void)
{
    return (cfg_file.flags & CFG_FLAG_AUTOSTART_D64) != 0;
}

static inline bool reu_enabled(void)
{
    return (cfg_file.flags & CFG_FLAG_REU_DISABLED) == 0;
}

static u8 get_device_number(u8 flags)
{
    u8 offset = flags & CFG_FLAG_DEVICE_D64_MSK;
    return (offset >> CFG_FLAG_DEVICE_D64_POS) + 8;
}

static void set_device_number(u8 *flags, u8 device)
{
    u8 offset = ((device - 8) << CFG_FLAG_DEVICE_D64_POS) &
                CFG_FLAG_DEVICE_D64_MSK;

    MODIFY_REG(*flags, CFG_FLAG_DEVICE_D64_MSK, offset);
}

static inline u8 device_number_d64(void)
{
    return get_device_number(cfg_file.flags);
}

static char * basic_get_filename(FILINFO *file_info)
{
    char *filename = file_info->fname;
    bool comma_found = false;

    size_t len;
    for (len=0; len<=16 && filename[len]; len++)
    {
        char c = ff_wtoupper(filename[len]);
        filename[len] = c;

        if (c == ',')
        {
            comma_found = true;
        }
    }

    if ((len > 16 || comma_found) && file_info->altname[0])
    {
        filename = file_info->altname;
    }

    return filename;
}

static void basic_load(const char *filename)
{
    u8 device = device_number_d64();

    // BASIC commands to run at start-up
    sprint((char *)KFF_BUF, "%cLOAD\"%s\",%u,1%cRUN\r%c", device,
           filename, device, 0, 0);
}

static void basic_no_commands(void)
{
    // No BASIC commands at start-up
    sprint((char *)KFF_BUF, "%c%c", device_number_d64(), 0);
}

static void basic_loading(const char *filename)
{
    // Setup string to print at BASIC start-up
    char *dest = (char *)crt_ram_buf + LOADING_OFFSET;
    dest = convert_to_screen_code(dest, "LOADING ");
    dest = convert_to_screen_code(dest, filename);
    *dest = 0;
}

static bool chdir_last(void)
{
    bool res = false;

    // Change to last selected dir if any
    if (cfg_file.path[0])
    {
        res = dir_change(cfg_file.path);
        if (!res)
        {
            cfg_file.path[0] = 0;
            cfg_file.file[0] = 0;
        }
    }
    else
    {
        cfg_file.file[0] = 0;
    }

    return res;
}

static void sanitize_sd_filename(char *dest, const char *src, u8 size)
{
    for (u8 i=0; i<size && *src; i++)
    {
        char c = *src++;
        if (c == '_')
        {
            c = 0xe4;
        }
        else
        {
            c = sanitize_char(c);
        }
        *dest++ = ff_wtoupper(c);
    }

    *dest = 0;
}

static void send_prg(u16 size, const char *name)
{
    basic_loading(name);
    if (!c64_send_prg(dat_buf, size))
    {
        system_restart();
    }
}

static u16 load_prg(char *name)
{
    if (!cfg_file.file[0] || !chdir_last())
    {
        return 0;
    }

    switch (cfg_file.img.mode)
    {
        case PRG_MODE_PRG:
        case PRG_MODE_P00:
        {
            FIL file;
            if (!file_open(&file, cfg_file.file, FA_READ))
            {
                return 0;
            }

            if (cfg_file.img.mode == PRG_MODE_P00)
            {
                return p00_load_file(&file, name);
            }

            // Limit filename to one line on the screen
            sanitize_sd_filename(name, cfg_file.file, 32);
            return prg_load_file(&file);
        }

        case PRG_MODE_D64:
        {
            D64_STATE *state = d64_open_image(cfg_file.file);
            if (!state)
            {
                return 0;
            }

            D64_DIR_ENTRY *entry = d64_find_element(state, cfg_file.img.element);
            if (!entry)
            {
                return 0;
            }

            d64_sanitize_filename(name, entry);
            return d64_read_prg(&state->d64, entry, dat_buf, sizeof(dat_buf));
        }

        case PRG_MODE_T64:
        {
            if (!t64_open(&t64_state.image, cfg_file.file) ||
                !t64_find_element(&t64_state, cfg_file.img.element))
            {
                return 0;
            }

            t64_sanitize_filename(name, &t64_state);
            return t64_read_prg(&t64_state.image, dat_buf, sizeof(dat_buf));
        }
    }

    return 0;
}

static bool load_crt(void)
{
    crt_buf_invalidate();
    dbg("Loading CRT");

    if (!cfg_file.file[0] || !chdir_last())
    {
        return false;
    }

    FIL file;
    if (!file_open(&file, cfg_file.file, FA_READ))
    {
        return false;
    }

    u8 banks;
    if (cfg_file.crt.flags & CRT_FLAG_ROM)
    {
        u16 len = rom_load_file(&file);
        if (!len)
        {
            return false;
        }
        banks = ((len - 1) / 16*1024) + 1;

        if (cfg_file.crt.game == 0 && cfg_file.crt.exrom == 1)
        {
            // Mirror Ultimax ROM if needed
            rom_mirror(len);
        }
    }
    else
    {
        CRT_HEADER header;
        if (!crt_load_header(&file, &header) ||
            !crt_is_supported(header.cartridge_type) ||
            !(banks = crt_load_file(&file, header.cartridge_type)))
        {
            return false;
        }

        crt_install_eapi(header.cartridge_type);
    }

    crt_buf_valid(banks);
    return true;
}

static bool load_disk(void)
{
    if (!chdir_last())
    {
        return false;
    }

    if (cfg_file.img.mode == DISK_MODE_D64)
    {
        D64_STATE *state = d64_open_image(cfg_file.file);
        if (!state)
        {
            return false;
        }

        if (cfg_file.img.element == ELEMENT_NOT_SELECTED)
        {
            if (autostart_d64())
            {
                basic_load("*");
            }
            else
            {
                basic_no_commands();
            }
        }
        else if (cfg_file.img.element < 2)
        {
            basic_load("*");
        }
        else
        {
            D64_DIR_ENTRY *entry = d64_find_element(state, cfg_file.img.element);
            if (!entry)
            {
                return false;
            }

            d64_sanitize_filename(scratch_buf, entry);
            basic_load(scratch_buf);
        }
    }
    else if (cfg_file.file[0])  // DISK_MODE_FS
    {
        FILINFO file_info;
        if (!file_stat(cfg_file.file, &file_info))
        {
            return false;
        }

        u8 file_type = get_file_type(&file_info);
        if (file_type == FILE_DIR)
        {
            if (!dir_change(cfg_file.file))
            {
                return false;
            }

            basic_no_commands();
        }
        else
        {
            char *filename = basic_get_filename(&file_info);
            basic_load(filename);
        }
    }
    else
    {
        basic_no_commands();
    }

    return true;
}

static bool load_txt(void)
{
    if (!cfg_file.file[0] || !chdir_last())
    {
        return false;
    }

    FIL file;
    if (!file_open(&file, cfg_file.file, FA_READ))
    {
        return false;
    }

    txt_load_file(&file);
    return true;
}

static void start_text_reader(void)
{
    format_path(scratch_buf, true);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    c64_send_text_reader((char *)&KFF_BUF[DIR_NAME_LENGTH+2],
                         sizeof(KFF_BUF) - (DIR_NAME_LENGTH+3));
}

static void c64_launcher_mode(void)
{
    crt_ptr = CRT_LAUNCHER;
    kff_init();
    C64_INSTALL_HANDLER(kff_handler);
}

static void c64_launcher_enable(void)
{
    c64_launcher_mode();
    c64_enable();
}

static void c64_launcher_reu_mode(void)
{
    crt_ptr = CRT_LAUNCHER;
    kff_reu_init();
    C64_INSTALL_HANDLER(kff_reu_handler);
}

static void c64_reu_mode(void)
{
    reu_init();
    C64_INSTALL_HANDLER(reu_handler);
}

static void c64_ef3_mode(void)
{
    ef_init();
    C64_INSTALL_HANDLER(ef3_handler);
}

static void c64_ef3_enable(void)
{
    c64_ef3_mode();
    c64_enable();
}

static bool c64_set_mode(void)
{
    if (!c64_is_reset())
    {
        // Disable VIC-II output if C64 has been started (needed for FC3)
        c64_interface_sync();
        c64_send_command(CMD_WAIT_RESET);
    }
    c64_disable();

    bool result = false;
    switch (cfg_file.boot_type)
    {
        case CFG_PRG:
        {
            u16 size = load_prg(scratch_buf);
            if (!size)
            {
                break;
            }

            c64_ef3_enable();
            send_prg(size, scratch_buf);
            result = true;
        }
        break;

        case CFG_CRT:
        {
            if (!crt_is_supported(cfg_file.crt.type))
            {
                break;
            }

            // Check if we have the CRT image in memory
            if (!crt_buf_is_valid() && !load_crt())
            {
                break;
            }

            c64_wait_valid_clock();
            crt_install_handler(&cfg_file.crt);
            c64_enable();
            result = true;
        }
        break;

        case CFG_USB:
        {
            c64_ef3_enable();
            basic_loading("FROM USB");
            result = true;
        }
        break;

        case CFG_DISK:
        {
            if (!load_disk())
            {
                break;
            }

            if (reu_enabled())
            {
                c64_launcher_reu_mode();
            }
            else
            {
                c64_launcher_mode();
            }
            c64_enable();

            result = true;
        }
        break;

        case CFG_TXT:
        {
            if (!load_txt())
            {
                break;
            }

            c64_launcher_enable();
            result = true;
        }
        break;

        case CFG_BASIC:
        {
            if (reu_enabled())
            {
                c64_reu_mode();
            }
            else
            {
                c64_ef3_mode();
            }

            // Unstoppable reset! - https://www.c64-wiki.com/wiki/Reset_Button
            C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
            c64_enable();
            delay_ms(300);
            C64_CRT_CONTROL(CRT_PORT_NONE);
            result = true;
        }
        break;

        case CFG_KILL:
        {
            // Also unstoppable!
            C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_8K);
            c64_reset_release();
            delay_ms(300);
            C64_CRT_CONTROL(CRT_PORT_NONE);
            result = true;
        }
        break;

        case CFG_BASIC_C128:
        {
            if (reu_enabled())
            {
                c64_reu_mode();
                c64_enable();
            }
            else
            {
                C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
                c64_reset_release();
            }
            result = true;
        }
        break;

        case CFG_DIAG:
        {
            c64_launcher_mode();
            result = true;
        }
    }

    return result;
}
