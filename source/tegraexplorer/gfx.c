#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/gfx.h"
#include "te.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "gfx.h"
#include "fs.h"
#include "../mem/minerva.h"

const char fixedoptions[3][50] = {
    "Folder -> previous folder                  ",
    "Clipboard -> Current folder                ",
    "Return to main menu                        "
};

const char sizevalues[4][3] = {
    "B ",
    "KB",
    "MB",
    "GB"
};

void clearscreen(){
    gfx_clear_grey(0x1B);
    gfx_box(0, 0, 719, 15, COLOR_WHITE);
    gfx_con_setpos(0, 0);
    gfx_printf("%k%KTegraexplorer%k%K\n", COLOR_DEFAULT, COLOR_WHITE, COLOR_WHITE, COLOR_DEFAULT);
}

int message(char* message, u32 color){
    clearscreen();
    gfx_printf("%k%s%k", color, message, COLOR_DEFAULT);
    return btn_wait();
}

int makemenu(menu_item menu[], int menuamount){
    int currentpos = 1, i, res;
    clearscreen();

    while (1){
        gfx_con_setpos(0, 31);

        if (currentpos == 1){
            while (currentpos < menuamount && menu[currentpos - 1].property < 1)
                currentpos++;
        }
        if (currentpos == menuamount){
            while (currentpos > 1 && menu[currentpos - 1].property < 1)
                currentpos--;
        }

        for (i = 0; i < menuamount; i++){
            if (menu[i].property < 0)
                continue;
            if (i == currentpos - 1)
                gfx_printf("%k%K%s%K\n", COLOR_DEFAULT, COLOR_WHITE, menu[i].name, COLOR_DEFAULT);
            else
                gfx_printf("%k%s\n", menu[i].color, menu[i].name);
        }
        gfx_printf("\n%k%s", COLOR_WHITE, menuamount);

        res = btn_wait();

        if (res & BTN_VOL_UP && currentpos > 1){
            currentpos--;
            while(menu[currentpos - 1].property < 1 && currentpos > 1)
                currentpos--;
        }
            
        else if (res & BTN_VOL_DOWN && currentpos < menuamount){
            currentpos++;
            while(menu[currentpos - 1].property < 1 && currentpos < menuamount)
                currentpos++;
        }

        else if (res & BTN_POWER)
            return menu[currentpos - 1].internal_function;
    }
}

void printbytes(u8 print[], u32 size, u32 offset){
    gfx_con_setpos(0, 31);
    gfx_hexdump(offset, print, size * sizeof(u8));
}

int makewaitmenu(char *initialmessage, char *hiddenmessage, int timer){
    clearscreen();
    int res;
    u32 start = get_tmr_s();

    gfx_printf(initialmessage);

    while(1){
        res = btn_read();

        if (res & BTN_VOL_DOWN || res & BTN_VOL_UP)
            return 0;

        if (start + timer > get_tmr_s())
            gfx_printf("\r<Wait %d seconds> ", timer + start - get_tmr_s());

        else if (res & BTN_POWER)
            return 1;

        else 
            gfx_printf("\r%k%s%k", COLOR_RED, hiddenmessage, COLOR_WHITE);
    }
}

void printfsentry(fs_entry file, bool highlight, bool refresh){
    int size = 0;
    char *display;
    int length;

    display = (char*) malloc (38);
    memset(display + 37, '\0', 1);
    
    if (strlen(file.name) > 37){
        strlcpy(display, file.name, 37);
        memset(display + 34, '.', 3);
    }
    else
        strcpy(display, file.name);

    if (highlight)
        gfx_printf("%K%k", COLOR_WHITE, COLOR_DEFAULT);
    else
        gfx_printf("%K%k", COLOR_DEFAULT, COLOR_WHITE);

    if (file.property & ISDIR)
        gfx_printf("%s\n", display);
    else {
        for (size = 4; size < 8; size++)
            if ((file.property & (1 << size)))
                break;

        gfx_printf("%k%s%K", COLOR_VIOLET, display, COLOR_DEFAULT);
    }

    if (refresh){
        length = strlen(display);
        for (int i = 0; i < (42 - length); i++)
            gfx_printf(" ");
    }

    if (!(file.property & ISDIR))
        gfx_printf("\a%d\e%s", file.size, sizevalues[size - 4]);

    free(display);
}

int makefilemenu(fs_entry *files, int amount, char *path){
    int currentpos = -2, i, res = 0, offset = 0, quickoffset = 300;
    u32 timer;
    bool refresh = false;
    clearscreen();
    gfx_con_setpos(544, 0);
    gfx_printf("%K%k%d / 500\n%K%k%s%k\n\n", COLOR_WHITE, COLOR_DEFAULT, amount, COLOR_DEFAULT, COLOR_GREEN, path, COLOR_DEFAULT);
    while (1){
        gfx_con_setpos(0, 47);
        timer = get_tmr_ms();
        for (i = -3 + offset; i < amount && i < 60 + offset; i++){
            if (i < 0){
                if (i == currentpos - 1)
                    gfx_printf("%k%K%s%K\n", COLOR_ORANGE, COLOR_WHITE, fixedoptions[i + 3], COLOR_DEFAULT);
                else
                    gfx_printf("%k%K%s\n", COLOR_ORANGE, COLOR_DEFAULT, fixedoptions[i + 3]);
            }
            else
                printfsentry(files[i], (i == currentpos - 1), refresh); 
        }
        refresh = false;
        gfx_printf("\n%k%K %s %s\n\nTime taken for screen draw: %dms ", COLOR_BLUE, COLOR_DEFAULT, (offset + 60 < amount) ? "v" : " ", (offset > 0) ? "^" : " ", get_tmr_ms() - timer);

        if (quickoffset == 300)
            res = btn_wait();
        else {
            msleep(quickoffset);
            res = btn_read();
        }

        if (res == 0)
            quickoffset = 300;
        else if (quickoffset > 46)
            quickoffset -= 45;

        if ((res & BTN_VOL_UP) && currentpos > -2){
            currentpos--;
            if (offset != 0 && currentpos < offset - 2){
                offset--;
                refresh = true;
            }
        }
        if ((res & BTN_VOL_DOWN) && currentpos < amount){
            currentpos++;
            if (currentpos - offset > 60){
                offset++;
                refresh = true;
            }
        }
        if (res & BTN_POWER)
            return currentpos;
    }
    minerva_periodic_training();
}