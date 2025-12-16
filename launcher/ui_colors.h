#ifndef UI_COLORS_H
#define UI_COLORS_H

#include <conio.h>

/* IDs must match firmware */
typedef enum {
    UI_COL_BLACK = 0,
    UI_COL_WHITE,
    UI_COL_RED,
    UI_COL_CYAN,
    UI_COL_PURPLE,
    UI_COL_GREEN,
    UI_COL_BLUE,
    UI_COL_YELLOW,
    UI_COL_ORANGE,
    UI_COL_BROWN,
    UI_COL_LIGHTRED,
    UI_COL_GRAY1,       // dark gray
    UI_COL_GRAY2,       // medium gray
    UI_COL_LIGHTGREEN,
    UI_COL_LIGHTBLUE,
    UI_COL_GRAY3,       // light gray
    UI_COL_MAX          // must equal 16 entries
} UI_COLOR_ID;

/* Extern global IDs (defined once in main.c) */
extern unsigned char g_color1_id;
extern unsigned char g_color2_id;
extern unsigned char g_color3_id;
extern unsigned char g_color4_id;
extern unsigned char g_color5_id;

/* Convert ID */
unsigned char ui_color_to_conio(unsigned char id);

/* Handy aliases for other files */
#define USER_COLOR1  ui_color_to_conio(g_color1_id)
#define USER_COLOR2  ui_color_to_conio(g_color2_id)
#define USER_COLOR3  ui_color_to_conio(g_color3_id)
#define USER_COLOR4  ui_color_to_conio(g_color4_id)
#define USER_COLOR5  ui_color_to_conio(g_color5_id)

#endif

