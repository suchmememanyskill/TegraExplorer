#include "tools.h"
#include "gfx.h"
#include "../libs/fatfs/ff.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"

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

    gfx_printf("%d KiB total\n%d KiB free\n\nPress any key to continue", tot_sect / 2, fre_sect / 2);


    btn_wait();
}