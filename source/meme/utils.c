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

void utils_waitforpower(){
    u32 btn = btn_wait();
    if (btn & BTN_VOL_UP)
        reboot_rcm();
    else if (btn & BTN_VOL_DOWN)
        reboot_normal();
    else
        power_off();
}

int readfolder(char *items[], unsigned int *muhbits, const char path[]){
    DIR dir;
    FILINFO fno;
    int i = 0;

    if (f_opendir(&dir, path)) {
        gfx_printf("\nFailed to open %s", path);
    }
    else{
        while (!f_readdir(&dir, &fno) && fno.fname[0]){
            size_t size = strlen(fno.fname) + 1;
            items[i] = (char*) malloc (size);
            strlcpy(items[i], fno.fname, size);
            if (fno.fattrib & AM_DIR) muhbits[i] |= (OPTION1);
            i++;
        }
    }
    return i;
}