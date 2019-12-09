#include "tools.h"
#include "gfx.h"
#include "../libs/fatfs/ff.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../soc/gpio.h"
#include "../utils/util.h"
#include "../utils/types.h"
#include "../libs/fatfs/diskio.h"
#include "../storage/sdmmc.h"

extern bool sd_mount();
extern void sd_unmount();
extern sdmmc_storage_t sd_storage;

void displayinfo(){
    clearscreen();

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect, temp_sect, sz_disk;
    s64 capacity;
    int res;

    gfx_printf("Getting storage info: please wait...");

    res = f_getfree("sd:", &fre_clust, &fs);
    gfx_printf("\nResult getfree: %d\n\n", res);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    gfx_printf("%d KiB total\n%d KiB free\n\nPress any key to continue\n", tot_sect / 2, fre_sect / 2);

    temp_sect = tot_sect;
    temp_sect -= 61145088;

    gfx_printf("\n%k1st part: %d\n2nd part: 61145088\n\n%k", COLOR_RED, temp_sect, COLOR_WHITE);

    capacity = sd_storage.csd.capacity;
    capacity -= 61145088;

    gfx_printf("\n%k1st part: %d\n2nd part: 61145088\n\n%k", COLOR_RED, capacity, COLOR_WHITE);

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
    bool fatalerror = false;

    u32 timer, totalsectors;
    BYTE work[FF_MAX_SS];
    DWORD plist[] = {666, 61145088};
    DWORD clustsize = 16 * 512;
    BYTE formatoptions = 0;
    formatoptions |= (FM_FAT32);
    formatoptions |= (FM_SFD);

    timer = get_tmr_s();

    totalsectors = sd_storage.csd.capacity;

    if (totalsectors < 61145088){
        gfx_printf("%k\nNot enough free space for emummc!", COLOR_RED);
        fatalerror = true;
    }

    if (!fatalerror){
        plist[0] = totalsectors - 61145088;
        gfx_printf("\nStarting SD partitioning:\nTotalSectors:        %d\nPartition1 (SD):     %d\nPartition2 (EMUMMC): %d\n", totalsectors, plist[0], plist[1]);
        gfx_printf("\nPartitioning SD...\n");
        res = f_fdisk(0, plist, &work);

        if (res){
            gfx_printf("%kf_fdisk returned %d!\n", COLOR_RED, res);
            fatalerror = true;
        }
        else
            gfx_printf("Done!\n");
    }

    if (!fatalerror){
        gfx_printf("\n\nFormatting Partition1...\n");
        res = f_mkfs("0:", formatoptions, clustsize, &work, sizeof work);

        if (res){
            gfx_printf("%kf_mkfs returned %d!\n", COLOR_RED, res);
            fatalerror = true;
        }
        else
            gfx_printf("Smells like a formatted SD\n\n");
    }

    sd_unmount();

    if (!fatalerror){
        if (!sd_mount())
            gfx_printf("%kSd failed to mount!\n", COLOR_ORANGE);
        else {
            gfx_printf("Sd mounted!\n");
        }
    }

    gfx_printf("\nPress any button to return%k\nTotal time taken: %ds", COLOR_WHITE, (get_tmr_s() - timer));
    btn_wait();
}