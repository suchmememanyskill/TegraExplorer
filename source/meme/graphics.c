#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "graphics.h"
#include "utils.h"

int _copystring(char *out, const char *in, int copynumb){
    strncpy(out, in, copynumb - 1);
    int strlength = strlen(in);
    if (strlength > copynumb + 1) strlength = copynumb;
    memset(out + strlength, '\0', 1);
    int ret = copynumb - strlength;
    return ret;
}

void _printwithhighlight(int offset, int folderamount, char *items[], int highlight, unsigned int *muhbits){
    char temp[39];
    int i = 0;
    int ret = 0;
    gfx_con_setpos(0, 32);
    while(i < folderamount && i < 76){
        ret = _copystring(temp, items[i + offset], 39);
        if(i == highlight - 1) gfx_printf("\n%k%p%s%k%p", COLOR_DEFAULT, COLOR_GREEN, temp, COLOR_GREEN, COLOR_DEFAULT);
        else gfx_printf("\n%s", temp);
        
        while(ret >= 0){
        gfx_printf(" ");
        ret = ret - 1;
        }
        
        gfx_con.x = 720 - (16 * 6);
        if (muhbits[i + offset] & OPTION1) gfx_printf("DIR");
        else gfx_printf("FILE");
        i++;
    }
}

int fileexplorergui(char *items[], unsigned int *muhbits, const char path[], int folderamount){
    bool change = true;
    int select = 1;
    int sleepvalue = 300;
    int offset = 0;
    char temp[43];
    gfx_con_setpos(0, 16);
    _copystring(temp, path, 43);
    gfx_printf("%k%s\n%k", COLOR_ORANGE, temp, COLOR_GREEN);
    while(1){
        if (change){
        _printwithhighlight(offset, folderamount, items, select, muhbits);
        change = false;
        msleep(sleepvalue);
        }

        u8 res = btn_read();
        if (res & BTN_VOL_UP){
            select = select - 1, change = true;
            sleepvalue = sleepvalue - 75;
        }
        else if (res & BTN_VOL_DOWN){
            select++, change = true;
            sleepvalue = sleepvalue - 75;
        }
        else {
            sleepvalue = 300;
        }
        if (res & BTN_POWER) break;
        if (select < 1){
            select = 1;
            if (offset > 0) offset = offset - 1;
        }
        if (select > folderamount) select = folderamount;
        if (select > 76){
            select = 76;
            if (76 + offset < folderamount) offset++;
        }
        if (sleepvalue < 30) sleepvalue = 30;
    }
    int ret = select + offset;
    return ret;
}