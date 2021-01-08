#pragma once
#include "gfx.h"
#include "menu.h"

#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_DEFAULT 0xFF1B1B1B
#define COLOR_GREY 0xFF888888
#define COLOR_DARKGREY 0xFF333333

#define COLORTORGB(color) (color & 0x00FFFFFF)
#define SETCOLOR(fg, bg) gfx_con_setcol(fg, 1, bg)
#define RESETCOLOR SETCOLOR(COLOR_WHITE, COLOR_DEFAULT);

#define RGBUnionToU32(optionUnion) (optionUnion | 0xFF000000)

void gfx_clearscreen();
int MakeHorizontalMenu(MenuEntry_t *entries, int len, int spacesBetween, u32 bg, int startPos);
int MakeYesNoHorzMenu(int spacesBetween, u32 bg);
void gfx_printTopInfo();