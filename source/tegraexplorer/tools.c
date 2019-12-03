#include "tools.h"
#include "gfx.h"
#include "../libs/fatfs/ff.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../soc/gpio.h"
#include "../utils/util.h"

void displayinfo(){
    clearscreen();

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    int res;

    gfx_printf("Getting storage info: please wait...");

    res = f_getfree("sd:", &fre_clust, &fs);
    gfx_printf("\nResult getfree: %d\n\n", res);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    gfx_printf("%d KiB total\n%d KiB free\n\nPress any key to continue\n", tot_sect / 2, fre_sect / 2);

    btn_wait();
}

void displaygpio(){
    int res;
    clearscreen();
    gfx_printf("Updates gpio pins ever 50ms:\nPress power to exit");
    msleep(200);
    while (1){
        msleep(10);
        gfx_con_setpos(0, 63);

        for (int i = 0; i <= 30; i++){
            gfx_printf("\nPort %d: ", i);
            for (int i2 = 7; i2 >= 0; i2--)
                gfx_printf("%d", gpio_read(i, (1 << i2)));
        }

        res = btn_read();
        if (res & BTN_POWER)
            break;
    }
}