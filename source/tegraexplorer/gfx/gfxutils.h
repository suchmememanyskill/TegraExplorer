#pragma once
#include "../../gfx/gfx.h"
#include "../../utils/types.h"

#define SWAPCOLOR(color) gfx_printf("%k", color)
#define SWAPBGCOLOR(color) gfx_printf("%K", color)
#define SWAPALLCOLOR(fg, bg) gfx_printf("%k%K", fg, bg)
#define RESETCOLOR gfx_printf("%k%K", COLOR_WHITE, COLOR_DEFAULT)

void gfx_clearscreen();
u32 gfx_message(u32 color, const char* message, ...);
u32 gfx_errDisplay(const char *src_func, int err, int loc);
int gfx_makewaitmenu(const char *hiddenmessage, int timer);
void gfx_printlength(int size, const char *toprint);
void gfx_printandclear(const char *in, int length, int endX);
void gfx_sideSetY(u32 setY);
u32 gfx_sideGetY();
void gfx_sideprintf(const char* message, ...);
void gfx_sideprintandclear(const char* message, int length);
void gfx_drawScrollBar(int minView, int maxView, int count);
int gfx_defaultWaitMenu(const char *message, int time);

extern int printerrors;