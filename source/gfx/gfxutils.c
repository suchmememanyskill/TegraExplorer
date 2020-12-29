#include "gfx.h"
#include "gfxutils.h"
#include <power/max17050.h>
#include "../hid/hid.h"

void gfx_clearscreen(){
    int battery = 0;
    max17050_get_property(MAX17050_RepSOC, &battery);

    //gfx_clear_grey(0x1B);
    gfx_boxGrey(0, 16, 1279, 703, 0x1b);
    SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);

    gfx_boxGrey(0, 703, 1279, 719, 0xFF);
    gfx_boxGrey(0, 0, 1279, 15, 0xFF);
    gfx_con_setpos(0, 0);
    gfx_printf("Tegraexplorer %d.%d.%d | Battery: %3d%%\n", LP_VER_MJ, LP_VER_MN, LP_VER_BF, battery >> 8);

    RESETCOLOR;
}

MenuEntry_t YesNoEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "No"},
    {.R = 255, .name = "Yes"}
};

int MakeYesNoHorzMenu(int spacesBetween, u32 bg){
    return MakeHorizontalMenu(YesNoEntries, ARR_LEN(YesNoEntries), spacesBetween, bg);
}

int MakeHorizontalMenu(MenuEntry_t *entries, int len, int spacesBetween, u32 bg){
    u32 initialX = 0, initialY = 0;
    u32 highlight = 0;
    gfx_con_getpos(&initialX, &initialY);

    while (1){
        for (int i = 0; i < len; i++){
            (highlight == i) ? SETCOLOR(bg, entries[i].optionUnion) : SETCOLOR(entries[i].optionUnion, bg);
            gfx_puts(entries[i].name);
            gfx_con.y -= spacesBetween * 16;
        }
        gfx_con_setpos(initialX, initialY);
        Input_t *input = hidWait();
        if (input->a)
            return highlight;
        else if (input->b)
            return 0;
        else if ((input->left || input->down) && highlight > 0)
            highlight--;
        else if ((input->right || input->up) && highlight < len - 1)
            highlight++;
    }
}