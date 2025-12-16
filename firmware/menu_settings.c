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


static const char *kColorNames[] = {
    "black",        // 0
    "white",        // 1
    "red",          // 2
    "cyan",         // 3
    "purple",       // 4
    "green",        // 5
    "blue",         // 6
    "yellow",       // 7
    "orange",       // 8
    "brown",        // 9
    "light red",    // 10
    "gray1",        // 11 (dark gray)
    "gray2",        // 12 (medium gray)
    "light green",  // 13
    "light blue",   // 14
    "gray3"         // 15 (light gray)
};

static u8 settings_flags;

static u8 settings_save(OPTIONS_STATE *state, OPTIONS_ELEMENT *element, u8 flags)
{
    cfg_file.flags = settings_flags;

    sd_send_prg_message("Saving settings.");
    save_cfg();
    restart_to_menu();

    return CMD_NONE;
}


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

static u8 color_next(u8 id){ return (u8)((id+1) % UI_COL_MAX); }

static const char *settings_color1_text(void){
    sprint(scratch_buf, "%24s: %s", "Frame color", kColorNames[cfg_file.ui_color1_id]);
    return scratch_buf;
}
static u8 settings_color1_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color1_id = color_next(cfg_file.ui_color1_id);
    return settings_refresh(e, settings_color1_text());
}

static const char *settings_color2_text(void){
    sprint(scratch_buf, "%24s: %s", "Line color", kColorNames[cfg_file.ui_color2_id]);
    return scratch_buf;
}
static u8 settings_color2_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color2_id = color_next(cfg_file.ui_color2_id);
    return settings_refresh(e, settings_color2_text());
}

static const char *settings_color3_text(void){
    sprint(scratch_buf, "%24s: %s", "Alt line color", kColorNames[cfg_file.ui_color3_id]);
    return scratch_buf;
}
static u8 settings_color3_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color3_id = color_next(cfg_file.ui_color3_id);
    return settings_refresh(e, settings_color3_text());
}

// --- Background color
static const char *settings_color4_text(void){
    sprint(scratch_buf, "%24s: %s", "Background color", kColorNames[cfg_file.ui_color4_id]);
    return scratch_buf;
}
static u8 settings_color4_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color4_id = color_next(cfg_file.ui_color4_id);
    return settings_refresh(e, settings_color4_text());
}

// --- Border color
static const char *settings_color5_text(void){
    sprint(scratch_buf, "%24s: %s", "Border color", kColorNames[cfg_file.ui_color5_id]);
    return scratch_buf;
}
static u8 settings_color5_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color5_id = color_next(cfg_file.ui_color5_id);
    return settings_refresh(e, settings_color5_text());
}

// --- Restore defaults (action row)
static const char *settings_defaultColor_text(void){
    return "Restore default colors";
}
static u8 settings_defaultColor_change(OPTIONS_STATE *s, OPTIONS_ELEMENT *e, u8 f){
    cfg_file.ui_color1_id = UI_COL_WHITE;
    cfg_file.ui_color2_id = UI_COL_WHITE;
    cfg_file.ui_color3_id = UI_COL_PURPLE;
    cfg_file.ui_color4_id = UI_COL_BLACK;
    cfg_file.ui_color5_id = UI_COL_BLACK;
  
    if (cfg_file.ui_color1_id >= UI_COL_MAX) cfg_file.ui_color1_id = UI_COL_WHITE;
    if (cfg_file.ui_color2_id >= UI_COL_MAX) cfg_file.ui_color2_id = UI_COL_WHITE;
    if (cfg_file.ui_color3_id >= UI_COL_MAX) cfg_file.ui_color3_id = UI_COL_PURPLE;
    if (cfg_file.ui_color4_id >= UI_COL_MAX) cfg_file.ui_color4_id = UI_COL_BLACK;
    if (cfg_file.ui_color5_id >= UI_COL_MAX) cfg_file.ui_color5_id = UI_COL_BLACK;

// Immediately persist + return to menu (same behavior as “Save”)
    return settings_save(s, e, f);
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


static u8 handle_settings(void)
{
    settings_flags = cfg_file.flags;

    OPTIONS_STATE *options = options_init("Settings");
    options_add_text_element(options, settings_basic_change,       settings_basic_text());
    options_add_text_element(options, settings_expansion_change,   settings_expansion_text());
    options_add_text_element(options, settings_autostart_change,   settings_autostart_text());
    options_add_text_element(options, settings_device_change,      settings_device_text());

    options_add_text_element(options, settings_color1_change,      settings_color1_text());
    options_add_text_element(options, settings_color2_change,      settings_color2_text());
    options_add_text_element(options, settings_color3_change,      settings_color3_text());
    options_add_text_element(options, settings_color4_change,      settings_color4_text());
    options_add_text_element(options, settings_color5_change,      settings_color5_text());

    options_add_text_element(options, settings_defaultColor_change, settings_defaultColor_text()); 

    options_add_text_element(options, settings_save, "Save");
    options_add_dir(options, "Cancel");
    return handle_options();
}

