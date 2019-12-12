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
#include "../utils/sprintf.h"
#include "emmc.h"
#include "fs.h"

extern bool sd_mount();
extern void sd_unmount();
extern sdmmc_storage_t sd_storage;

void displayinfo(){
    clearscreen();

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    u32 capacity;
    int res;

    gfx_printf("Biskeys:\n");
    print_biskeys();

    if (!sd_mount()){
        gfx_printf("SD mount failed!\nFailed to display SD info\n");
    }
    else {
        gfx_printf("Getting storage info: please wait...");

        res = f_getfree("sd:", &fre_clust, &fs);
        gfx_printf("\nResult getfree: %d\n\n", res);

        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        capacity = sd_storage.csd.capacity;

        gfx_printf("Entire sd:\nSectors: %d\nSpace total: %d MB\n\n", capacity, capacity / 2048);
        gfx_printf("First partition on SD:\nSectors: %d\nSpace total: %d MB\nSpace free: %d MB\n\n", tot_sect, tot_sect / 2048, fre_sect / 2048);
    }

    gfx_printf("Press any key to continue");
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

int dumpfirmware(){
    DIR dir;
    FILINFO fno;
    bool fail = false;
    int ret, amount = 0;
    char path[100] = "emmc:/Contents/registered";
    char sdfolderpath[100] = "";
    char syspath[100] = "";
    char sdpath[100] = "";
    short pkg1ver = returnpkg1ver();
    u32 timer = get_tmr_s();

    clearscreen();

    gfx_printf("PKG1 version: %d\n", pkg1ver);

    ret = f_mkdir("sd:/tegraexplorer");
    gfx_printf("Creating sd:/tegraexplorer %d\n", ret);

    ret = f_mkdir("sd:/tegraexplorer/Firmware");
    gfx_printf("Creating sd:/tegraexplorer/Firmware %d\n", ret);

    sprintf(sdfolderpath, "sd:/tegraexplorer/Firmware/%d", pkg1ver);
    ret = f_mkdir(sdfolderpath);
    gfx_printf("Creating %s %d\n", sdfolderpath, ret);

    ret = f_opendir(&dir, path);
    gfx_printf("Result opening system:/ %d\n\n%k", ret, COLOR_GREEN);

    while(!f_readdir(&dir, &fno) && fno.fname[0] && !fail){
        sprintf(sdpath, "%s/%s", sdfolderpath, fno.fname);

        if (fno.fattrib & AM_DIR)
            sprintf(syspath, "%s/%s/00", path, fno.fname);
        else
            sprintf(syspath, "%s/%s", path, fno.fname);

        ret = copy(syspath, sdpath, false);

        gfx_printf("%d %s\r", ++amount, fno.fname);

        if (ret != 0)
            fail = true;
    }

    if (fail)
        gfx_printf("%k\n\nDump failed! Aborting (%d)", COLOR_RED, ret);

    gfx_printf("%k\n\nPress any button to continue...\nTime taken: %ds", COLOR_WHITE, get_tmr_s() - timer);

    btn_wait();

    return fail;
}

int format(int mode){
    clearscreen();
    int res;
    bool fatalerror = false;
    DWORD plist[] = {666, 61145088};
    u32 timer, totalsectors;
    BYTE work[FF_MAX_SS];
    DWORD clustsize = 32768;
    BYTE formatoptions = 0;
    formatoptions |= (FM_FAT32);
    //formatoptions |= (FM_SFD);

    disconnect_emmc();

    timer = get_tmr_s();
    totalsectors = sd_storage.csd.capacity;

    if (mode == 0){
        if (totalsectors < 61145088){
            gfx_printf("%k\nNot enough free space for emummc!", COLOR_RED);
            fatalerror = true;
        }

        if (!fatalerror){
            plist[0] = totalsectors - 61145088;
            gfx_printf("\nStarting SD partitioning:\nTotalSectors:        %d\nPartition1 (SD):     %d\nPartition2 (EMUMMC): %d\n", totalsectors, plist[0], plist[1]);
        }
    }
    else {
        plist[0] = totalsectors;
        plist[1] = 0;
    }

    if (!fatalerror){
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

    dump_biskeys();
    mount_emmc("SYSTEM", 2);

    gfx_printf("\nPress any button to return%k\nTotal time taken: %ds", COLOR_WHITE, (get_tmr_s() - timer));
    btn_wait();
    return fatalerror;
}