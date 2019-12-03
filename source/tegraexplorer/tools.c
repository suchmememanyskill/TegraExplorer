#include "tools.h"
#include "gfx.h"
#include "../libs/fatfs/ff.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../soc/gpio.h"
#include "../utils/util.h"
#include "../utils/types.h"

extern bool sd_mount();
extern void sd_unmount();

void displayinfo(){
    clearscreen();

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect, temp_sect;
    int res;

    gfx_printf("Getting storage info: please wait...");

    res = f_getfree("sd:", &fre_clust, &fs);
    gfx_printf("\nResult getfree: %d\n\n", res);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    gfx_printf("%d KiB total\n%d KiB free\n\nPress any key to continue\n", tot_sect / 2, fre_sect / 2);

    temp_sect = tot_sect;
    temp_sect -= 61145088;

    gfx_printf("\n1st part: %d\n2nd part: 61145088", temp_sect);

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

void format(){
    clearscreen();
    int res;

    u32 timer;
    FATFS *fs;
    DWORD fre_clust, tot_sect, temp_sect;
    BYTE work[FF_MAX_SS];
    DWORD plist[] = {188928257, 61145089, 0};
    DWORD sectsize = 16 * 512;
    BYTE formatoptions = 0;
    formatoptions |= (FM_FAT32);
    formatoptions |= (FM_SFD);

    timer = get_tmr_s();
    gfx_printf("Getting free cluster info from 1st partition");

    if (res = f_getfree("sd:", &fre_clust, &fs))
        gfx_printf("%kGetfree failed! errcode %d", COLOR_RED, res);
    else {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        temp_sect = tot_sect;
        temp_sect -= 61145089;
        gfx_printf("Total sectors: %d\nFree after emummc: %d\n\n", tot_sect, temp_sect);
        if (temp_sect < 0)
            gfx_printf("%kNot enough space free!\n", COLOR_RED);
        else {
            gfx_printf("Partitioning sd...\nPartition1 size: %d\nPartition2 size: %d\n", plist[0], plist[1]);
            plist[0] = temp_sect;
            if (res = f_fdisk(0, plist, &work))
                gfx_printf("Partitioning failed! errcode %d\n", res);
            else {
                gfx_printf("\nFormatting FAT32 Partition:\n");
                if (res = f_mkfs("0:", formatoptions, sectsize, &work, sizeof work))
                    gfx_printf("%kFormat failed! errcode %d\n", res);
                else {
                    gfx_printf("Smells like a formatted SD\n\n");
                }
            } 
        }
    }

    if (!res){
        sd_unmount();
        if (!sd_mount())
            gfx_printf("%kSd failed to mount!\n", COLOR_ORANGE);
        else {
            gfx_printf("Sd mounted!\n");
        }
    }

    gfx_printf("\nPress any button to return%k\nTotal time taken: %ds", COLOR_WHITE, (get_tmr_s() - timer));
    btn_wait();

    /*
    res = f_fdisk(0, plist, &work);
    gfx_printf("f_fdisk partdrive: %d\n", res);

    if (!res){
        res = f_mkfs("0:", muhoptions, sectsize, &work, sizeof work);
        gfx_printf("f_mkfs0 res: %d\n", res);
    }

    sd_unmount();
    res = sd_mount();

    gfx_printf("sd_mount res: %s", (res ? "1" : "0"));
    btn_wait();
    */
}