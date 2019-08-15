#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "utils.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"

void utils_gfx_init(){
    display_backlight_brightness(100, 1000);
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
}

void removepartpath(char *path){
    char *ret;
    ret = strrchr(path, '/');
    memset(ret, '\0', 1);
    if (strcmp(path, "sd:") == 0) strcpy(path, "sd:/");
}

void addpartpath(char *path, char *add){
    if (strcmp(path, "sd:/") != 0) strcat(path, "/");
    strcat(path, add);
}

void utils_waitforpower(){
    u32 btn = btn_wait();
    if (btn & BTN_VOL_UP)
        reboot_rcm();
    else if (btn & BTN_VOL_DOWN)
        reboot_normal();
    else
        power_off();
}

void _addchartoarray(char *add, char *items[], int spot){
    size_t size = strlen(add) + 1;
    items[spot] = (char*) malloc (size);
    strlcpy(items[spot], add, size);
}

void _mallocandaddfolderbit(unsigned int *muhbits, int spot, bool value){
    muhbits[spot] = (unsigned int) malloc (sizeof(int));
    if (value) muhbits[spot] |= (OPTION1);
}

int readfolder(char *items[], unsigned int *muhbits, const char *path){
    DIR dir;
    FILINFO fno;
    int i = 2;
    _addchartoarray(".", items, 0);
    _addchartoarray("..", items, 1);
    _mallocandaddfolderbit(muhbits, 0, true);
    _mallocandaddfolderbit(muhbits, 1, true);

    if (f_opendir(&dir, path)) {
        gfx_printf("\nFailed to open %s", path);
    }
    else {
        while (!f_readdir(&dir, &fno) && fno.fname[0]){
            _addchartoarray(fno.fname, items, i);
            _mallocandaddfolderbit(muhbits, i, fno.fattrib & AM_DIR);
            i++;
        }
    }
    f_closedir(&dir);
    return i;
}