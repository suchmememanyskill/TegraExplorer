#include "gfx.h"
#include "gfxutils.h"
#include <power/max17050.h>

void gfx_clearscreen(){
    int battery = 0;
    max17050_get_property(MAX17050_RepSOC, &battery);

    //gfx_clear_grey(0x1B);
    gfx_boxGrey(0, 16, 1279, 703, 0x1b);
    SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);

    gfx_boxGrey(0, 703, 1279, 719, 0xFF);
    gfx_boxGrey(0, 0, 1279, 15, 0xFF);
    gfx_con_setpos(0, 0);
    gfx_printf("Tegraexplorer Rewrite | Battery: %3d%%\n", battery >> 8);

    RESETCOLOR;
}

u32 FromRGBtoU32(u8 r, u8 g, u8 b){
    return 0xFF000000 | r << 16 | g << 8 | b;
}