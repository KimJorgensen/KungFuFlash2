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

static u8 settings_flags;

static u8 settings_refresh(OPTIONS_ELEMENT *element, const char *text)
{
    options_element_text(element, text);
    return menu->dir(menu->state);  // Refresh settings
}

static const char * setting_print(const char *setting, const char *value)
{
    sprint(scratch_buf, "%24s: %s", setting, value);
    return scratch_buf;
}

static const char * settings_basic_text(void)
{
    return setting_print("Persist BASIC selection",
        settings_flags & CFG_FLAG_NO_PERSIST ? "no" : "yes");
}

static u8 settings_basic_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    if (settings_flags & CFG_FLAG_NO_PERSIST)
    {
        settings_flags &= ~CFG_FLAG_NO_PERSIST;
    }
    else
    {
        settings_flags |= CFG_FLAG_NO_PERSIST;
    }

    return settings_refresh(element, settings_basic_text());
}

static const char * settings_expansion_text(void)
{
    return setting_print("RAM expansion (REU)",
        settings_flags & CFG_FLAG_REU_DISABLED ? "no" : "yes");
}

static u8 settings_expansion_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    if (settings_flags & CFG_FLAG_REU_DISABLED)
    {
        settings_flags &= ~CFG_FLAG_REU_DISABLED;
    }
    else
    {
        settings_flags |= CFG_FLAG_REU_DISABLED;
    }

    return settings_refresh(element, settings_expansion_text());
}

static const char * settings_autostart_text(void)
{
    return setting_print("Autostart disk image",
        settings_flags & CFG_FLAG_AUTOSTART_D64 ? "yes" : "no");
}

static u8 settings_autostart_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    if (settings_flags & CFG_FLAG_AUTOSTART_D64)
    {
        settings_flags &= ~CFG_FLAG_AUTOSTART_D64;
    }
    else
    {
        settings_flags |= CFG_FLAG_AUTOSTART_D64;
    }

    return settings_refresh(element, settings_autostart_text());
}

static const char * settings_device_text(void)
{
    sprint(scratch_buf+ELEMENT_LENGTH, "%u", get_device_number(settings_flags));
    return setting_print("Disk device number", scratch_buf+ELEMENT_LENGTH);
}

static u8 settings_device_change(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    u8 device = get_device_number(settings_flags) + 1;
    set_device_number(&settings_flags, device);

    return settings_refresh(element, settings_device_text());
}

static u8 settings_save(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    cfg_file.flags = settings_flags;

    sd_send_prg_message("Saving settings.");
    save_cfg();
    restart_to_menu();

    return CMD_NONE;
}

static u8 handle_settings(void)
{
    settings_flags = cfg_file.flags;

    OPTIONS_STATE *options = options_init("Settings");
    options_add_text_element(options, settings_basic_change, settings_basic_text());
    options_add_text_element(options, settings_expansion_change, settings_expansion_text());
    options_add_text_element(options, settings_autostart_change, settings_autostart_text());
    options_add_text_element(options, settings_device_change, settings_device_text());
    options_add_text_element(options, settings_save, "Save");
    options_add_dir(options, "Cancel");
    return handle_options();
}
