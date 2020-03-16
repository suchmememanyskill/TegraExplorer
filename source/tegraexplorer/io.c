#include <string.h>
#include "../mem/heap.h"
#include "gfx.h"
#include "fs.h"
#include "../utils/types.h"
#include "../libs/fatfs/ff.h"
#include "../utils/sprintf.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"
#include "../utils/util.h"
#include "io.h"
#include "emmc.h"
#include "../storage/sdmmc.h"
#include "../storage/nx_emmc.h"
#include "../utils/types.h"
#include "../storage/emummc.h"

extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;

bool checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    return !(fr & FR_NO_FILE);
}

void viewbytes(char *path){
    FIL in;
    u8 print[2048];
    u32 size;
    QWORD offset = 0;
    int res;

    clearscreen();
    res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        message(COLOR_RED, "File Opening Failed\nErrcode %d", res);
        return;
    }

    msleep(200);

    while (1){
        f_lseek(&in, offset * 16);

        res = f_read(&in, &print, 2048 * sizeof(u8), &size);
        if (res != FR_OK){
            message(COLOR_RED, "Reading Failed!\nErrcode %d", res);
            return;
        }

        printbytes(print, size, offset * 16);
        res = btn_read();

        if (!res)
            res = btn_wait();

        if (res & BTN_VOL_DOWN && 2048 * sizeof(u8) == size)
            offset++;
        if (res & BTN_VOL_UP && offset > 0)
            offset--;
        if (res & BTN_POWER)
            break;
    }
    f_close(&in);
}

int copy(const char *locin, const char *locout, bool print, bool canCancel){
    FIL in, out;
    FILINFO in_info;
    u64 sizeoffile, sizecopied = 0, totalsize;
    UINT temp1, temp2;
    u8 *buff;
    unsigned int x, y, i = 0;
    int res;

    gfx_con_getpos(&x, &y);

    if (!strcmp(locin, locout)){
        return 21;
    }

    if ((res = f_open(&in, locin, FA_READ | FA_OPEN_EXISTING))){
        return 22;
    }

    if (f_stat(locin, &in_info))
        return 22;

    if (f_open(&out, locout, FA_CREATE_ALWAYS | FA_WRITE)){
        return 23;
    }

    buff = malloc (BUFSIZE);
    sizeoffile = f_size(&in);
    totalsize = sizeoffile;

    while (sizeoffile > 0){
        if ((res = f_read(&in, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp1)))
            return res;
        if ((res = f_write(&out, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp2)))
            return res;

        if (temp1 != temp2)
            return 24;

        sizeoffile -= temp1;
        sizecopied += temp1;

        if (print && 10 > i++){
            gfx_printf("%k[%d%%]%k", COLOR_GREEN, ((sizecopied * 100) / totalsize) ,COLOR_WHITE);
            gfx_con_setpos(x, y);
            
            i = 0;

            if (canCancel)
                if (btn_read() & BTN_VOL_DOWN){
                    f_unlink(locout);
                    break;
                }
        }
    }

    f_close(&in);
    f_close(&out);
    free(buff);

    if ((res = f_chmod(locout, in_info.fattrib, 0x3A)))
        return res;

    f_stat(locin, &in_info); //somehow stops fatfs from being weird
    return 0;
}

u64 getfilesize(char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}

int getfolderentryamount(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0;

    if ((f_opendir(&dir, path))){
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        folderamount++;
    }

    f_closedir(&dir);
    return folderamount;
}

void makestring(char *in, char **out){
    *out = (char *) malloc (strlen(in) + 1);
    strcpy(*out, in);
}

int del_recursive(char *path){
    DIR dir;
    FILINFO fno;
    int res;
    u32 x, y;
    char *localpath = NULL;

    gfx_con_getpos(&x, &y);
    makestring(path, &localpath);

    if ((res = f_opendir(&dir, localpath))){
        //message(COLOR_RED, "Error during f_opendir: %d", res);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            del_recursive(getnextloc(localpath, fno.fname));
        }
        else {
            gfx_box(0, y, 719, y + 16, COLOR_DEFAULT);
            SWAPCOLOR(COLOR_RED);
            gfx_printf("\r");
            gfx_print_length(37, fno.fname);
            gfx_printf(" ");

            if ((res = f_unlink(getnextloc(localpath, fno.fname))))
                return res;
        }
    }

    f_closedir(&dir);

    if ((res = f_unlink(localpath))){
        return res;
    }

    free(localpath);

    return 0;
}

int copy_recursive(char *path, char *dstpath){
    DIR dir;
    FILINFO fno;
    int res;
    u32 x, y;
    char *startpath = NULL, *destpath = NULL, *destfoldername = NULL, *temp = NULL;

    gfx_con_getpos(&x, &y);

    makestring(path, &startpath);
    makestring(strrchr(path, '/') + 1, &destfoldername);

    destpath = (char*) malloc (strlen(dstpath) + strlen(destfoldername) + 2);
    sprintf(destpath, (dstpath[strlen(dstpath) - 1] == '/') ? "%s%s" : "%s/%s", dstpath, destfoldername);

    if ((res = f_opendir(&dir, startpath))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return 21;
    }

    f_mkdir(destpath);

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            copy_recursive(getnextloc(startpath, fno.fname), destpath);
        }
        else {
            gfx_box(0, y, 719, y + 16, COLOR_DEFAULT);
            SWAPCOLOR(COLOR_GREEN);
            gfx_printf("\r");
            gfx_print_length(37, fno.fname);
            gfx_printf(" ");
            makestring(getnextloc(startpath, fno.fname), &temp);

            if ((res = copy(temp, getnextloc(destpath, fno.fname), true, false))){
                return res;
            }

            free(temp);
        }
    }
    
    f_closedir(&dir);
    
    if (f_stat(startpath, &fno))
        return 22;

    if ((res = f_chmod(destpath, fno.fattrib, 0x3A)))
        return res;
    
    free(startpath);
    free(destpath);
    free(destfoldername);

    return 0;
}

int dump_emmc_part(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part){
    FIL fp;
    u8 *buf;
	u32 lba_curr = part->lba_start;
	u32 bytesWritten = 0;
    u32 totalSectors = part->lba_end - part->lba_start + 1;
    u64 totalSize = (u64)((u64)totalSectors << 9);
    u32 num = 0;
    u32 pct = 0;
    int res;

    gfx_printf("Initializing\r");
    buf = calloc(16384, sizeof(u8));

    if ((res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE))){
        message(COLOR_RED, "f_open() failed! err: %d", res);
        return -1;
    }

    f_lseek(&fp, totalSize);
    f_lseek(&fp, 0);

    while (totalSectors > 0){
        num = MIN(totalSectors, 32);
        if (!emummc_storage_read(mmcstorage, lba_curr, num, buf)){
            message(COLOR_RED, "eMMC read failed!");
            return -1;
        }
        if ((res = f_write(&fp, buf, num * NX_EMMC_BLOCKSIZE, NULL))){
            message(COLOR_RED, "f_write() failed! err: %d", res);
            return -1;
        }
        pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
        gfx_printf("Progress: %d%%\r", pct);

        lba_curr += num;
        totalSectors -= num;
        bytesWritten += num * NX_EMMC_BLOCKSIZE;
    }
    gfx_printf("                \r");
    f_close(&fp);
    free(buf);
    return 0;
}

int dump_emmc_parts(u16 parts, u8 mmctype){
    char *path;
    char basepath[] = "sd:/tegraexplorer/dumps";
    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/dumps");

    connect_mmc(mmctype);
    clearscreen();

    if (parts & PART_BOOT){
        emmc_part_t bootPart;
        const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
        memset(&bootPart, 0, sizeof(bootPart));
        
        bootPart.lba_start = 0;
        bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
        
        for (int i = 0; i < 2; i++){
            strcpy(bootPart.name, "BOOT");
            bootPart.name[4] = (u8)('0' + i);
		    bootPart.name[5] = 0;     

            sdmmc_storage_set_mmc_partition(&storage, i + 1);
            makestring(getnextloc(basepath, bootPart.name), &path);
            gfx_printf("Dumping %s\n", bootPart.name);
 
            dump_emmc_part(path, &storage, &bootPart);
            free(path);
        }
    }

    if (parts & PART_PKG2){
        for (int i = 0; i < 6; i++){
            if (connect_part(pkg2names[i])){
                message(COLOR_RED, "Find of partition failed!\nPart: %s", pkg2names[i]);
                return -1;
            }

            makestring(getnextloc(basepath, system_part->name), &path);
            gfx_printf("Dumping %s\n", system_part->name);

            dump_emmc_part(path, &storage, system_part);
            free(path);
        }
    }

    gfx_printf("\nDone!");
    btn_wait();
    return 0;
}

int restore_emmc_part(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part){
    FIL fp;
    FILINFO fno;
    u8 *buf;
	u32 lba_curr = part->lba_start;
	u32 bytesWritten = 0;
    u32 totalSectorsDest = part->lba_end - part->lba_start + 1;
    u64 totalSizeDest = (u64)((u64)totalSectorsDest << 9);
    u64 totalSize;
    u32 num = 0;
    u32 pct = 0;
    int res;

    gfx_printf("Initializing\r");

    buf = calloc(16384, sizeof(u8));

    if (!buf){
        message(COLOR_RED, "Calloc returned null!");
        return -1;
    }

    if ((res = f_stat(path, &fno))){
        message(COLOR_RED, "f_stat() failed! err: %d", res);
        return -1;
    }

    totalSize = fno.fsize;
    u64 totalSectors = totalSize / NX_EMMC_BLOCKSIZE;

    if (totalSize > totalSizeDest){
        message(COLOR_RED, "File too big for destenation!");
        return -1;
    }

    gfx_printf("Flashing %s\n", part->name);

    if (totalSize < totalSizeDest){
        SWAPCOLOR(COLOR_ORANGE);
        u8 btnres = makewaitmenunoclear(
            "File is too small for destenation.\nDo you want to flash it anyway?\n\nVol +/- to Cancel\n",
            "Power to Confirm",
            2
        );

        RESETCOLOR;

        if (!btnres){
            gfx_printf("\nCancelled: %s\n\n", part->name);
            return 0;
        }
        else
            gfx_printf("\nFlashing %s\n", part->name);
    }

    if ((res = f_open(&fp, path, FA_OPEN_ALWAYS | FA_READ))){
        message(COLOR_RED, "f_open() failed! err: %d", res);
        return -1;
    }

    while (totalSectors > 0){
        num = MIN(totalSectors, 32);

        if ((res = f_read(&fp, buf, num * NX_EMMC_BLOCKSIZE, NULL))){
            message(COLOR_RED, "f_read() failed! err: %d", res);
            return -1;
        }

        if (!emummc_storage_write(mmcstorage, lba_curr, num, buf)){
            message(COLOR_RED, "eMMC write failed!");
            return -1;
        }

        lba_curr += num;
        totalSectors -= num;
        bytesWritten += num * NX_EMMC_BLOCKSIZE;

        pct = (u64)((u64)(bytesWritten) * 100u) / (u64)(fno.fsize);
        gfx_printf("Progress: %d%%\r", pct);
    }

    gfx_printf("                \r");
    f_close(&fp);
    free(buf);
    return 0;
}

int restore_emmc_file(char *path, const char *target, u8 partition, u8 mmctype){
    connect_mmc(mmctype);

    if (partition){
        emmc_part_t bootPart;
        const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
        memset(&bootPart, 0, sizeof(bootPart));

        bootPart.lba_start = 0;
        bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;

        strcpy(bootPart.name, target);

        sdmmc_storage_set_mmc_partition(&storage, partition);
        restore_emmc_part(path, &storage, &bootPart);
    }
    else {
        if (connect_part(target)){
            message(COLOR_RED, "Find of partition failed!\nPart: %s", target);
            return -1;
        }
        restore_emmc_part(path, &storage, system_part);
    }
    return 0;
}

int restore_bis_using_file(char *path, u8 mmctype){
    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/bis");
    clearscreen();

    if (extract_bis_file(path, "sd:/tegraexplorer/bis")){
        message(COLOR_ORANGE, "Failed to find file!");
        return -1;
    }

    if (!makewaitmenunoclear(
        "\nAre you sure you want to flash these files\nThis could leave your switch unbootable!\n\nVol +/- to cancel\n",
        "Power to confirm",
        5
    ))
    {
        return 0;
    }

    gfx_printf("\nRestoring BIS...\n\n");

    restore_emmc_file("sd:/tegraexplorer/bis/BOOT0.bin", "BOOT0", 1, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BOOT1.bin", "BOOT1", 2, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-1-Normal-Main", pkg2names[0], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-1-Normal-Main", pkg2names[1], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-3-SafeMode-Main", pkg2names[2], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-3-SafeMode-Main", pkg2names[3], 0, mmctype);

    gfx_printf("\n\nDone!\nPress any button to return");
    btn_wait();

    return 0;
}

u32 DecodeInt(u8* data) {
    u32 out = 0;

    for (int i = 0; i < 4; i++) {
        out |= (data[i] << ((3 - i) * 8));
    }

    return out;
}

void copy_fil_size(FIL* in_src, FIL* out_src, int size_src){
    u8* buff;
    buff = calloc(16384, sizeof(u8));

    int size = size_src;
    int copysize;

    while (size > 0){
        copysize = MIN(16834, size);
        f_read(in_src, buff, copysize, NULL);
        f_write(out_src, buff, copysize, NULL);
        size -= copysize;
    }
}

void gen_part(int size, FIL* in, char *path){
    FIL out;

    f_open(&out, path, FA_WRITE | FA_CREATE_ALWAYS);
    copy_fil_size(in, &out, size);
    f_close(&out);
}

int extract_bis_file(char *path, char *outfolder){
    FIL in;
    int res;
    u8 version[0x10];
    u8 args;
    u8 temp[0x4];
    u32 filesizes[4];

    if ((res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING))){
        message(COLOR_RED, "Opening file failed! err: %d", res);
        return -1;
    }

    f_read(&in, version, 0x10, NULL);
    f_read(&in, &args, 1, NULL);

    for (int i = 0; i < 4; i++){
        f_read(&in, temp, 4, NULL);
        filesizes[i] = DecodeInt(temp);
    }

    gfx_printf("Version: %s\n\n", version);
    
    /*
    for (int i = 0; i < 4; i++) {
        gfx_printf("%d ", filesizes[i]);
    }
    */

    gfx_printf("\nExtracting BOOT0\n");
    if (args & BOOT0_ARG)
        gen_part(filesizes[0], &in, getnextloc(outfolder, "BOOT0.bin"));

    gfx_printf("Extracting BOOT1\n");
    if (args & BOOT1_ARG)
        gen_part(filesizes[1], &in, getnextloc(outfolder, "BOOT1.bin"));

    gfx_printf("Extracting BCPKG2_1\n");
    if (args & BCPKG2_1_ARG)
        gen_part(filesizes[2], &in, getnextloc(outfolder, "BCPKG2-1-Normal-Main"));

    gfx_printf("Extracting BCPKG2_3\n");
    if (args & BCPKG2_3_ARG)
        gen_part(filesizes[3], &in, getnextloc(outfolder, "BCPKG2-3-SafeMode-Main"));

    gfx_printf("\n\nDone!\n");

    f_close(&in);
    return 0;   
}