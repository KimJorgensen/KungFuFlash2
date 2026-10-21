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

// From https://vice-emu.sourceforge.io/vice_17.html
typedef enum
{
    CRT_NORMAL_CARTRIDGE = 0x00,
    CRT_ACTION_REPLAY,
    CRT_KCS_POWER_CARTRIDGE,
    CRT_FINAL_CARTRIDGE_III,
    CRT_SIMONS_BASIC,
    CRT_OCEAN_TYPE_1,
    CRT_EXPERT_CARTRIDGE,
    CRT_FUN_PLAY_POWER_PLAY,
    CRT_SUPER_GAMES,
    CRT_ATOMIC_POWER,
    CRT_EPYX_FASTLOAD,
    CRT_WESTERMANN_LEARNING,
    CRT_REX_UTILITY,
    CRT_FINAL_CARTRIDGE_I,
    CRT_MAGIC_FORMEL,
    CRT_C64_GAME_SYSTEM_SYSTEM_3,
    CRT_WARP_SPEED,
    CRT_DINAMIC,
    CRT_ZAXXON_SUPER_ZAXXON,
    CRT_MAGIC_DESK_DOMARK_HES_AUSTRALIA,
    CRT_SUPER_SNAPSHOT_V5,
    CRT_COMAL_80,
    CRT_STRUCTURED_BASIC,
    CRT_ROSS,
    CRT_DELA_EP64,
    CRT_DELA_EP7X8,
    CRT_DELA_EP256,
    CRT_REX_EP256,
    CRT_MIKRO_ASSEMBLER,
    CRT_FINAL_CARTRIDGE_PLUS,
    CRT_ACTION_REPLAY_4,
    CRT_STARDOS,
    CRT_EASYFLASH,
    CRT_EASYFLASH_XBANK,
    CRT_CAPTURE,
    CRT_ACTION_REPLAY_3,
    CRT_RETRO_REPLAY,
    CRT_MMC64,
    CRT_MMC_REPLAY,
    CRT_IDE64,
    CRT_SUPER_SNAPSHOT_V4,
    CRT_IEEE_488,
    CRT_GAME_KILLER,
    CRT_PROPHET64,
    CRT_EXOS,
    CRT_FREEZE_FRAME,
    CRT_FREEZE_MACHINE,
    CRT_SNAPSHOT64,
    CRT_SUPER_EXPLODE_V5_0,
    CRT_MAGIC_VOICE,
    CRT_ACTION_REPLAY_2,
    CRT_MACH_5,
    CRT_DIASHOW_MAKER,
    CRT_PAGEFOX,
    CRT_KINGSOFT,
    CRT_SILVERROCK_128K_CARTRIDGE,
    CRT_FORMEL_64,
    CRT_RGCD,
    CRT_RR_NET_MK3,
    CRT_EASYCALC,
    CRT_GMOD2,
    CRT_MAX_BASIC,
    CRT_GMOD3,
    CRT_ZIPP_CODE_48,
    CRT_BLACKBOX_V8,
    CRT_BLACKBOX_V3,
    CRT_BLACKBOX_V4,
    CRT_REX_RAM_FLOPPY,
    CRT_BIS_PLUS,
    CRT_SD_BOX,
    CRT_MULTIMAX,
    CRT_BLACKBOX_V9,
    CRT_LT_KERNAL_HOST_ADAPTOR,
    CRT_RAMLINK,
    CRT_DREAN,
    CRT_IEEE_FLASH_64,
    CRT_TURTLE_GRAPHICS_II,
	CRT_FREEZE_FRAME_MK2,
    CRT_PARTNER_64,

    // KFF specific extensions
    CRT_C128_CARTRIDGE = 0x8000,
    CRT_C128_NORMAL_CARTRIDGE = CRT_C128_CARTRIDGE,
    CRT_C128_WARP_SPEED,
    CRT_C128_PARTNER_128,
    CRT_C128_COMAL_80,
    CRT_C128_MAGIC_DESK_128,
    CRT_C128_GMOD2
} CRT_TYPE;

typedef enum
{
    CRT_CHIP_ROM = 0x00,
    CRT_CHIP_RAM,
    CRT_CHIP_FLASH
} CRT_CHIP_TYPE;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u8 signature[16];
    u32 header_length;
    u16 version;
    u16 cartridge_type;
    u8 exrom;
    u8 game;
    u8 hardware_revision;
    u8 reserved[5];
    u8 cartridge_name[32];
} CRT_HEADER;

typedef struct
{
    u8 signature[4];
    u32 packet_length;
    u16 chip_type;
    u16 bank;
    u16 start_address;
    u16 image_size;
} CRT_CHIP_HEADER;

typedef struct
{
    char signature[8];
    char filename[17];
    u8 rel_record_size;
} P00_HEADER;

typedef struct
{
    char magic[4];
    u16 version;
    u16 data_offset;
    u16 load_address;
    u16 init_address;
    u16 play_address;
    u16 songs;
    u16 start_song;
    u32 speed;
    char name[32];
    char author[32];
    char released[32];
} SID_HEADER;
#pragma pack(pop)

typedef enum
{
    FILE_DIR        = 0x00,
    FILE_DIR_UP,
    FILE_CRT,
    FILE_PRG,
    FILE_P00,
    FILE_D64,
    FILE_D64_STAR,
    FILE_D64_PRG,
    FILE_T64,
    FILE_T64_PRG,
    FILE_SID,
    FILE_ROM,
    FILE_TXT,

    FILE_UPD        = 0xfe,
    FILE_UNKNOWN
} FILE_TYPE;

typedef enum
{
    CFG_FLAG_NO_PERSIST         = 0x01,
    CFG_FLAG_REU_DISABLED       = 0x02,

    CFG_FLAG_AUTOSTART_D64      = 0x10,
    CFG_FLAG_DEVICE_NUM_D64_1   = 0x20,
    CFG_FLAG_DEVICE_NUM_D64_2   = 0x40,
    CFG_FLAG_DEVICE_NUM_D64_3   = 0x80
} CFG_FLAGS;

#define CFG_FLAG_DEVICE_D64_POS 0x05
#define CFG_FLAG_DEVICE_D64_MSK (0x07 << CFG_FLAG_DEVICE_D64_POS)

typedef enum
{
    CFG_NONE = 0x00,
    CFG_CRT,
    CFG_PRG,
    CFG_USB,
    CFG_DISK,
    CFG_TXT,
    CFG_BASIC,
    CFG_KILL,
    CFG_BASIC_C128,
    CFG_DIAG
} CFG_BOOT_TYPE;

typedef enum
{
    CRT_FLAG_NONE       = 0x00,
    CRT_FLAG_ROM        = 0x01, // CRT is loaded from a ROM file

    CRT_FLAG_VIC        = 0x80  // Support VIC/C128 2MHz mode reads (EF only)
} CFG_CRT_FLAGS;

typedef enum
{
    DISK_MODE_FS    = 0x00, // Use filesystem
    DISK_MODE_D64   = 0x01  // Use D64 disk image
} CFG_DISK_MODE;

typedef enum
{
    PRG_MODE_PRG    = 0x00,
    PRG_MODE_P00    = 0x01,
    PRG_MODE_D64    = 0x02,
    PRG_MODE_T64    = 0x03
} CFG_PRG_MODE;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u16 type;           // CRT_TYPE
    u8 hw_rev;          // Cartridge hardware revision
    u8 exrom;           // EXROM line status
    u8 game;            // GAME line status
    u8 flags;           // CFG_CRT_FLAGS
} CFG_CRT_HEADER;

typedef struct
{
    u8 mode;            // CFG_PRG_MODE or CFG_DISK_MODE
    u16 element;        // Used if file is a T64 or D64
} CFG_IMG_HEADER;

typedef struct
{
    u8 signature[5];    // CFG_SIGNATURE
    s8 phi2_offset;
    u8 flags;           // CFG_FLAGS
    u32 reserved;       // Should be 0
    u8 boot_type;       // CFG_BOOT_TYPE

    union
    {
        CFG_CRT_HEADER crt; // boot_type is CFG_CRT
        CFG_IMG_HEADER img; // boot_type is CFG_PRG or CFG_DISK
    };

    char path[750];
    char file[256];
} CFG_FILE;
#pragma pack(pop)

#define CFG_SIGNATURE "KFF2\1"
#define ELEMENT_NOT_SELECTED 0xffff

#define UPD_FILE_SIZE   (128*1024)
#define UPD_FILE_VER    (112*1024)

static CFG_FILE cfg_file;
