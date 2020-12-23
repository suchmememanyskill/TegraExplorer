#pragma once
#include "gfx.h"

#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_DEFAULT 0xFF1B1B1B

#define COLORTORGB(color) (color & 0x00FFFFFF)
#define SETCOLOR(fg, bg) gfx_con_setcol(fg, 1, bg)
#define RESETCOLOR SETCOLOR(COLOR_WHITE, COLOR_DEFAULT);

u32 FromRGBtoU32(u8 r, u8 g, u8 b);

void gfx_clearscreen();