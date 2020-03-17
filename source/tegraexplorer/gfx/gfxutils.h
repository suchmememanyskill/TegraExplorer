#pragma once
#include "../../gfx/gfx.h"
#include "../../utils/types.h"

#define SWAPCOLOR(color) gfx_printf("%k", color)
#define SWAPBGCOLOR(color) gfx_printf("%K", color)
#define RESETCOLOR gfx_printf("%k%K", COLOR_WHITE, COLOR_DEFAULT)

void gfx_clearscreen();
int gfx_message(u32 color, const char* message, ...);
int gfx_errprint(u32 color, int func, int err, int add);
int gfx_makewaitmenu(char *hiddenmessage, int timer);
void gfx_printlength(int size, char *toprint);
void gfx_printandclear(char *in, int length);
void gfx_printfilesize(int size, char *type);