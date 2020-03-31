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

int printerrors = true;

void gfx_clearscreen(){
    int battery = 0;
    max17050_get_property(MAX17050_RepSOC, &battery);

    gfx_clear_grey(0x1B);
    SWAPCOLOR(COLOR_DEFAULT);
    SWAPBGCOLOR(COLOR_WHITE);

    gfx_box(0, 1263, 719, 1279, COLOR_WHITE);
    gfx_con_setpos(0, 1263);
    gfx_printf("Move: Vol+/- | Select: Pow | Battery: %3d%%", battery >> 8);

    gfx_box(0, 0, 719, 15, COLOR_WHITE);
    gfx_con_setpos(0, 0);
    gfx_printf("Tegraexplorer v1.4.0\n");

    RESETCOLOR;
}

int gfx_message(u32 color, const char* message, ...){
    va_list ap;
    va_start(ap, message);

    gfx_clearscreen();
    SWAPCOLOR(color);

    gfx_vprintf(message, ap);

    va_end(ap);
    return btn_wait();
}

int gfx_errDisplay(char *src_func, int err, int loc){
    if (!printerrors)
        return 0;

    gfx_clearscreen();
    SWAPCOLOR(COLOR_ORANGE);
    gfx_printf("\nAn error occured:\n\n");
    gfx_printf("Function: %s\nErrcode: %d\n", src_func, err);
    
    if (err < 15)
        gfx_printf("Desc: %s\n", utils_err_codes[err]);
    else if (err >= ERR_SAME_LOC && err <= ERR_PARSE_FAIL)
        gfx_printf("Desc: %s\n", utils_err_codes_te[err - 50]);

    if (loc)
        gfx_printf("Loc: %d\n", loc);

    gfx_printf("\nPress any button to return");

    RESETCOLOR;
    return btn_wait();
}

int gfx_makewaitmenu(char *hiddenmessage, int timer){
    int res;
    u32 start = get_tmr_s();

    while (btn_read() != 0);

    while(1){
        res = btn_read();

        if (res & BTN_VOL_DOWN || res & BTN_VOL_UP)
            return 0;

        if (start + timer > get_tmr_s())
            gfx_printf("\r<Wait %d seconds> ", timer + start - get_tmr_s());

        else if (res & BTN_POWER)
            return 1;

        else 
            gfx_printf("\r%s", hiddenmessage);
    }
}

void gfx_printlength(int size, char *toprint){
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

void gfx_printandclear(char *in, int length){
    u32 x, y;

    gfx_printlength(length, in);
    gfx_con_getpos(&x, &y);
    RESETCOLOR;

    for (int i = (703 - x) / 16; i > 0; i--)
        gfx_printf(" ");

    gfx_con_setpos(x, y);

    //gfx_box(x, y, 719, y + 16, COLOR_DEFAULT);
    /*
    u8 color = 0x1B;
    gfx_set_rect_grey(&color, 719 - x, 16, x, y);
    */
}

void gfx_printfilesize(int size, char *type){
    SWAPCOLOR(COLOR_VIOLET);
    gfx_printf("\a%4d\e%s", size, type);
    RESETCOLOR;
}