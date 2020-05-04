#include "gfxutils.h"
#include <string.h>
#include <stdarg.h>
#include "menu.h"
#include "../../gfx/gfx.h"
#include "../../power/max17050.h"
#include "../../utils/btn.h"
#include "../../utils/util.h"
#include "../../mem/heap.h"
#include "../common/common.h"
#include "../../hid/hid.h"

int printerrors = true;

void gfx_clearscreen(){
    int battery = 0;
    max17050_get_property(MAX17050_RepSOC, &battery);

    gfx_clear_grey(0x1B);
    SWAPCOLOR(COLOR_DEFAULT);
    SWAPBGCOLOR(COLOR_WHITE);

    gfx_boxGrey(0, 703, 1279, 719, 0xFF);
    gfx_boxGrey(0, 0, 1279, 15, 0xFF);
    gfx_con_setpos(0, 0);
    gfx_printf("Tegraexplorer v2.0.0 | Battery: %3d%%\n", battery >> 8);

    RESETCOLOR;
}

u32 gfx_message(u32 color, const char* message, ...){
    va_list ap;
    va_start(ap, message);

    gfx_clearscreen();
    SWAPCOLOR(color);

    gfx_vprintf(message, ap);

    va_end(ap);
    return hidWait()->buttons;
}

u32 gfx_errDisplay(const char *src_func, int err, int loc){
    if (!printerrors)
        return 0;

    gfx_clearscreen();
    SWAPCOLOR(COLOR_ORANGE);
    gfx_printf("\nAn error occured:\n\n");
    gfx_printf("Function: %s\nErrcode: %d\n", src_func, err);
    
    if (err < 15)
        gfx_printf("Desc: %s\n", utils_err_codes[err]);
    else if (err >= ERR_SAME_LOC && err <= ERR_NO_DESTINATION)
        gfx_printf("Desc: %s\n", utils_err_codes_te[err - 50]);

    if (loc)
        gfx_printf("Loc: %d\n", loc);

    gfx_printf("\nPress any button to return\n");

    RESETCOLOR;

    return hidWait()->buttons;
}

int gfx_makewaitmenu(const char *hiddenmessage, int timer){
    u32 start = get_tmr_s();
    Inputs *input = NULL;

    while(1){
        input = hidRead();

        if (input->buttons & (KEY_VOLM | KEY_VOLP | KEY_B))
            return 0;

        if (start + timer > get_tmr_s())
            gfx_printf("\r<Wait %d seconds> ", timer + start - get_tmr_s());

        else if (input->a)
            return 1;

        else 
            gfx_printf("\r%s", hiddenmessage);
    }
}

void gfx_printlength(int size, const char *toprint){
    char *temp;
    temp = (char*) malloc (size + 1);

    if (strlen(toprint) > size){
        strlcpy(temp, toprint, size);
        memset(temp + size - 3, '.', 3);
        memset(temp + size, '\0', 1);
    }
    else
        strcpy(temp, toprint);

    gfx_printf("%s", temp);
    free(temp);
}

void gfx_printandclear(const char *in, int length, int endX){
    u32 x, y;

    gfx_printlength(length, in);
    gfx_con_getpos(&x, &y);
    RESETCOLOR;


    /*
    for (int i = (703 - x) / 16; i > 0; i--)
        gfx_printf(" ");
    */

    gfx_boxGrey(x, y, endX, y + 15, 0x1B);

    gfx_con_setpos(x, y);

    //gfx_box(x, y, 719, y + 16, COLOR_DEFAULT);
    /*
    u8 color = 0x1B;
    gfx_set_rect_grey(&color, 719 - x, 16, x, y);
    */
}

static u32 sideY = 0;
void _gfx_sideSetYAuto(){
    u32 getX, getY;
    gfx_con_getpos(&getX, &getY);
    sideY = getY;
}

void gfx_sideSetY(u32 setY){
    sideY = setY;
}

u32 gfx_sideGetY(){
    return sideY;
}

void gfx_sideprintf(const char* message, ...){
    va_list ap;
    va_start(ap, message);

    gfx_con_setpos(800, sideY);
    gfx_vprintf(message, ap);
    _gfx_sideSetYAuto();

    va_end(ap);
}

void gfx_sideprintandclear(const char* message, int length){
    gfx_con_setpos(800, sideY);
    gfx_printandclear(message, length, 1279);
    gfx_putc('\n');
    _gfx_sideSetYAuto();
}

void gfx_drawScrollBar(int minView, int maxView, int count){
    int curScrollCount = 1 + maxView - minView;
    if (curScrollCount >= count)
        return;

    u32 barSize = (703 * (curScrollCount * 1000 / count)) / 1000;
    u32 offsetSize = (703 * (minView * 1000 / count)) / 1000;

    gfx_boxGrey(740, 16, 755, 702, 0x1B);
    if ((16 + barSize + offsetSize) > 702)
        gfx_boxGrey(740, 16 + offsetSize, 755, 702, 0x66);
    else
        gfx_boxGrey(740, 16 + offsetSize, 755, 16 + barSize + offsetSize, 0x66);
}

int gfx_defaultWaitMenu(const char *message, int time){
    gfx_clearscreen();
    SWAPCOLOR(COLOR_ORANGE);
    gfx_printf("\n%s\n\nPress B to return\n", message);
    SWAPCOLOR(COLOR_RED);
    return gfx_makewaitmenu("Press A to continue", time);
}