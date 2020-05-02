#pragma once
#include "../../gfx/gfx.h"
#include "../../utils/types.h"

#define SWAPCOLOR(color) gfx_printf("%k", color)
#define SWAPBGCOLOR(color) gfx_printf("%K", color)
#define RESETCOLOR gfx_printf("%k%K", COLOR_WHITE, COLOR_DEFAULT)

void gfx_clearscreen();
u32 gfx_message(u32 color, const char* message, ...);
u32 gfx_errDisplay(char *src_func, int err, int loc);
int gfx_makewaitmenu(char *hiddenmessage, int timer);
void gfx_printlength(int size, char *toprint);
void gfx_printandclear(char *in, int length, int endX);
void gfx_printfilesize(int size, char *type);
void gfx_sideSetY(u32 setY);
u32 gfx_sideGetY();
void gfx_sideprintf(char* message, ...);
void gfx_sideprintandclear(char* message, int length);
void gfx_drawScrollBar(int minView, int maxView, int count);

extern int printerrors;