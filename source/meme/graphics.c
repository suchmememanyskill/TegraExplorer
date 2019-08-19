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
    return ret + 4;
}

int messagebox(char *message){
    int ret = -1;
    meme_clearscreen();
    gfx_printf("%s", message);
    msleep(100);
    u8 res = btn_wait();
        if (res & BTN_POWER) ret = 0;
        else ret = 1;
    meme_clearscreen();
    return ret;
}

int gfx_menulist(int ypos, char *list[], int length){
    int i = 0;
    int highlight = 1;
    while(1){
        gfx_con_setpos(0, ypos);
        while(i < length){
            if (i == highlight - 1) gfx_printf("%k%p%s%k%p\n", COLOR_DEFAULT, COLOR_WHITE, list[i], COLOR_WHITE, COLOR_DEFAULT);
            else gfx_printf("%s\n", list[i]);
            i++;
        }
        i = 0;
        u8 res = btn_wait();
        if (res & BTN_VOL_UP) highlight--;
        else if (res & BTN_VOL_DOWN) highlight++;
        else if (res & BTN_POWER) break;
        if (highlight < 1) highlight = 1;
        if (highlight > length) highlight = length;
    }
    return highlight;
}

void meme_clearscreen(){
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    gfx_box(0, 0, 719, 15, COLOR_WHITE);
    gfx_printf("%k%pTegraExplorer\n%k%p", COLOR_DEFAULT, COLOR_WHITE, COLOR_WHITE, COLOR_DEFAULT);
}

void _printwithhighlight(int offset, int folderamount, char *items[], int highlight, unsigned int *muhbits, int *filesizes){
    char temp[39];
    int i = 0;
    int ret = 0; 
    gfx_con_setpos(0, 32);
    while(i < folderamount && i < 76){
        ret = _copystring(temp, items[i + offset], 39);
        if(i == highlight - 1) gfx_printf("\n%k%p%s%k%p", COLOR_DEFAULT, COLOR_WHITE, temp, COLOR_WHITE, COLOR_DEFAULT);
        else if ((i == 0 || i == 1) && offset == 0) gfx_printf("%k\n%s%k", COLOR_ORANGE, temp, COLOR_WHITE);
        else if (muhbits[i+offset] & OPTION1) gfx_printf("\n%s", temp);
        else gfx_printf("%k\n%s%k", COLOR_VIOLET, temp, COLOR_WHITE);

        while(ret >= 0){
        gfx_printf(" ");
        ret = ret - 1;
        }
        
        gfx_con.x = 720 - (16 * 6);
        if (!(muhbits[i + offset] & OPTION1)) { //should change later
            char temp[6];
            return_readable_byte_amounts(filesizes[i + offset], temp);
            gfx_printf("%s", temp);
        }
        i++;
    }
}

int fileexplorergui(char *items[], unsigned int *muhbits, const char path[], int folderamount){
    bool change = true;
    int select = 1;
    int sleepvalue = 300;
    int offset = 0;
    char temp[43];
    int *filesizes;
    int i = 0;
    filesizes = (int*) calloc(500, sizeof(int));
    gfx_con_setpos(0, 48);
    for (i = 0; i < folderamount; i++){
        if(!(muhbits[i] & OPTION1)){
            char temppath[PATHSIZE];
            strcpy(temppath, path);
            strcat(temppath, "/");
            strcat(temppath, items[i]);
            filesizes[i] = getfilesize(temppath);
            gfx_printf("Calcing filesizes: %d / %d\r", i, folderamount - 2);
        }
    }
    _copystring(temp, path, 43);
    gfx_con_setpos(0, 16);
    gfx_printf("%k%s\n%k", COLOR_GREEN, temp, COLOR_WHITE);
    while(1){
        if (change){
        _printwithhighlight(offset, folderamount, items, select, muhbits, filesizes);
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
    free(filesizes);
    return ret;
}