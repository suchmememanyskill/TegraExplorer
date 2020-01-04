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

fs_entry *fileobjects;
char rootpath[10] = "";
char *currentpath = "";
char *clipboard = "";
u8 clipboardhelper = 0;
extern const char sizevalues[4][3];
extern int launch_payload(char *path);

menu_item explfilemenu[9] = {
    {"-- File Menu --", COLOR_BLUE, -1, 0},
    {"FILE", COLOR_GREEN, -1, 0},
    {"\nSIZE", COLOR_VIOLET, -1, 0},
    {"\n\n\nBack", COLOR_WHITE, -1, 1},
    {"\nCopy to clipboard", COLOR_BLUE, COPY, 1},
    {"Move to clipboard", COLOR_BLUE, MOVE, 1},
    {"\nDelete file\n", COLOR_RED, DELETE, 1},
    {"Launch Payload", COLOR_ORANGE, PAYLOAD, 1},
    {"View Hex", COLOR_GREEN, HEXVIEW, 1}
};

menu_item explfoldermenu[5] = {
    {"-- Folder Menu --\n", COLOR_BLUE, -1, 0},
    {"Back", COLOR_WHITE, -1, 1},
    {"Return to main menu\n", COLOR_BLUE, EXITFOLDER, 1},
    {"Copy to clipboard", COLOR_VIOLET, COPYFOLDER, 1},
    {"Delete folder", COLOR_RED, DELETEFOLDER, 1}
};

void writecurpath(const char *in){
    if (currentpath != NULL)
        free(currentpath);

    size_t len = strlen(in) + 1;
    currentpath = (char*) malloc (len);
    strcpy(currentpath, in);

   strcpy(currentpath, in);
}

void writeclipboard(const char *in, bool move, bool folder){
    if (clipboard != NULL)
        free(clipboard);

    clipboardhelper = 0;
    
    if (move)
        clipboardhelper |= (OPERATIONMOVE);
    else
        clipboardhelper |= (OPERATIONCOPY);

    if (folder)
        clipboardhelper |= (ISDIR);

    size_t len = strlen(in) + 1;
    clipboard = (char*) malloc (len);
    strcpy(clipboard, in);
    
   strcpy(clipboard, in);
}

char *getnextloc(char *current, char *add){
    static char *ret;

    if (ret != NULL){
        free(ret);
        ret = NULL;
    }

    size_t size = strlen(current) + strlen(add) + 1;
    ret = (char*) malloc (size);
    if (!strcmp(rootpath, current))
        sprintf(ret, "%s%s", current, add);
    else
        sprintf(ret, "%s/%s", current, add);

    return ret;
}

char *getprevloc(char *current){
    static char *ret;
    char *temp;

    if (ret != NULL){
        free(ret);
        ret = NULL;
    }

    size_t size = strlen(current) + 1;

    ret = (char*) malloc (size);
    strcpy(ret, current);

    temp = strrchr(ret, '/');
    memset(temp, '\0', 1);

    if (strlen(rootpath) > strlen(ret))
        strcpy(ret, rootpath);

    return ret;
}

fs_entry getfileobj(int spot){
    return fileobjects[spot];
}

bool checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    if (fr & FR_NO_FILE)
        return false;
    else
        return true;
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

    gfx_con_getpos(&x, &y);

    if (!strcmp(locin, locout)){
        return 3;
    }

    if (f_open(&in, locin, FA_READ | FA_OPEN_EXISTING)){
        return 1;
    }

    if (f_stat(locin, &in_info))
        return 1;

    if (f_open(&out, locout, FA_CREATE_ALWAYS | FA_WRITE)){
        return 2;
    }

    buff = malloc (BUFSIZE);
    sizeoffile = f_size(&in);
    totalsize = sizeoffile;

    while (sizeoffile > 0){
        if (f_read(&in, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp1))
            return 3;
        if (f_write(&out, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp2))
            return 4;

        if (temp1 != temp2)
            return 5;

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

    if (in_info.fattrib & AM_ARC){
        f_chmod(locout, AM_ARC, AM_ARC);
    }

    f_close(&in);
    f_close(&out);
    free(buff);

    return 0;
}

void copyfile(const char *path, const char *outfolder){
    char *filename = strrchr(path, '/');
    char *outstring;
    size_t outstringsize = strlen(filename) + strlen(outfolder) + 2;
    int res;

    clearscreen();

    outstring = (char*) malloc (outstringsize);

    if (strcmp(rootpath, outfolder))
        sprintf(outstring, "%s/%s", outfolder, filename + 1);
    else
        sprintf(outstring, "%s%s", outfolder, filename + 1);

    gfx_printf("Note:\nTo stop the transfer hold Vol-\n\n%s\nProgress: ", outstring);

    if (clipboardhelper & OPERATIONMOVE){
        if (strcmp(rootpath, "emmc:/"))
            f_rename(path, outstring);
        else
            message(COLOR_RED, "\nMoving in emummc is not allowed!");
    }
    
    else if (clipboardhelper & OPERATIONCOPY) {
        res = copy(path, outstring, true, true);
        if (res){
            gfx_printf("\n\n%kSomething went wrong while copying!\n\nErrcode: %d%k", COLOR_RED, res, COLOR_WHITE);
            btn_wait();
        }
    }

    else {
        message(COLOR_RED, "\nClipboard is empty!");
    }

    free (outstring);
    clipboardhelper = 0;
}

u64 getfilesize(char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}

void addobject(char* name, int spot, bool isfol, bool isarc){
    size_t length = strlen(name) + 1;
    u64 size = 0;
    int sizes = 0;
    fileobjects[spot].property = 0;

    if (fileobjects[spot].name != NULL){
        free(fileobjects[spot].name);
        fileobjects[spot].name = NULL;
    }

    fileobjects[spot].name = (char*) malloc (length);
    strlcpy(fileobjects[spot].name, name, length);

    if (isfol)
        fileobjects[spot].property |= (ISDIR);
    else {
        size = getfilesize(getnextloc(currentpath, name));
        
        while (size > 1024){
            size /= 1024;
            sizes++;
        }

        if (sizes > 3)
            sizes = 0;

        fileobjects[spot].property |= (1 << (4 + sizes));
        fileobjects[spot].size = size;
    }

    if (isarc)
        fileobjects[spot].property |= (ISARC);
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

void clearfileobjects(){
    if (fileobjects != NULL){
        for (int i = 0; fileobjects[i].name != NULL; i++){
            free(fileobjects[i].name);
            fileobjects[i].name = NULL;
        }
        free(fileobjects);
        fileobjects = NULL;
    }
}

void createfileobjects(int size){
    fileobjects = calloc (size + 1, sizeof(fs_entry));
    fileobjects[size].name = NULL;
}

int readfolder(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0, res;
    
    clearfileobjects();
    createfileobjects(getfolderentryamount(path));

    if ((res = f_opendir(&dir, path))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        addobject(fno.fname, folderamount++, (fno.fattrib & AM_DIR), (fno.fattrib & AM_ARC));
    }

    f_closedir(&dir);
    return folderamount;
}

int delfile(const char *path, const char *filename){
    char *tempmessage;
    size_t tempmessagesize = strlen(filename) + 65;
    tempmessage = (char*) malloc (tempmessagesize);

    sprintf(tempmessage, "Are you sure you want to delete:\n%s\n\nPress vol+/- to cancel\n", filename);
    if (makewaitmenu(tempmessage, "Press power to delete", 3)){
        f_unlink(path);
        return readfolder(currentpath);
    }
    else
        return -1;
}

void makestring(char *in, char **out){
    *out = (char *) malloc (strlen(in) + 1);
    strcpy(*out, in);
}

int del_recursive(char *path){
    DIR dir;
    FILINFO fno;
    int res;
    char *localpath = NULL;
    makestring(path, &localpath);

    if ((res = f_opendir(&dir, localpath))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR)
            del_recursive(getnextloc(localpath, fno.fname));

        else {
            gfx_box(0, 47, 719, 63, COLOR_DEFAULT);
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
    char *startpath = NULL, *destpath = NULL, *destfoldername = NULL, *temp = NULL;

    makestring(path, &startpath);
    makestring(strrchr(path, '/') + 1, &destfoldername);

    destpath = (char*) malloc (strlen(dstpath) + strlen(destfoldername) + 2);
    sprintf(destpath, (dstpath[strlen(dstpath) - 1] == '/') ? "%s%s" : "%s/%s", dstpath, destfoldername);
    

    if ((res = f_opendir(&dir, startpath))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return -1;
    }

    f_mkdir(destpath);

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            copy_recursive(getnextloc(startpath, fno.fname), destpath);
        }
        else {
            gfx_box(0, 47, 719, 63, COLOR_DEFAULT);
            SWAPCOLOR(COLOR_GREEN);
            gfx_printf("\r");
            gfx_print_length(37, fno.fname);
            gfx_printf(" ");
            makestring(getnextloc(startpath, fno.fname), &temp);

            if ((res = copy(temp, getnextloc(destpath, fno.fname), true, false))){
                message(COLOR_RED, "Copy failed!\nErrcode %d", res);
                return -1;
            }

            free(temp);
        }
    }

    if (f_stat(startpath, &fno))
        return -2;

    if (fno.fattrib & AM_ARC){
        f_chmod(destpath, AM_ARC, AM_ARC);
    }
    
    f_closedir(&dir);
    free(startpath);
    free(destpath);
    free(destfoldername);

    return 0;
}

void copyfolder(char *in, char *out){
    bool fatalerror = false;
    int res;

    if (!strcmp(in, rootpath)){
        message(COLOR_RED, "In is root\nAborting!");
        fatalerror = true;
    }
    
    if (strstr(out, in) != NULL  && !fatalerror){
        message(COLOR_RED, "\nOut is a part of in!\nAborting");
        fatalerror = true;
    }

    if (!fatalerror){
        clearscreen();
        gfx_printf("\nCopying folder, please wait\n");
        if ((res = copy_recursive(in, out)))
            message(COLOR_RED, "copy_recursive() failed!\nErrcode %d", res);
    }

    clipboardhelper = 0;
}

int filemenu(fs_entry file){
    int temp;
    strlcpy(explfilemenu[1].name, file.name, 43);
            
    for (temp = 4; temp < 8; temp++)
        if ((file.property & (1 << temp)))
            break;

    sprintf(explfilemenu[2].name, "\nSize: %d %s", file.size, sizevalues[temp - 4]);

    if (strstr(file.name, ".bin") != NULL && file.size & ISKB){
        explfilemenu[7].property = 1;
    }
    else
        explfilemenu[7].property = -1;

    temp = makemenu(explfilemenu, 9);

    switch (temp){
        case COPY:
            writeclipboard(getnextloc(currentpath, file.name), false, false);
            break;
        case MOVE:
            writeclipboard(getnextloc(currentpath, file.name), true, false);
            break;
        case DELETE:
            if ((temp = delfile(getnextloc(currentpath, file.name), file.name)) != -1)
                return temp + 1;
        case PAYLOAD:
            launch_payload(getnextloc(currentpath, file.name));
            break;
        case HEXVIEW:
            viewbytes(getnextloc(currentpath, file.name));
            break;
    }

    return 0;
}

int foldermenu(){
    int res;

    res = makemenu(explfoldermenu, 5);

    switch (res){
        case EXITFOLDER:
            return -1;
        case DELETEFOLDER:
            if (makewaitmenu("Do you want to delete this folder?\nThe entire folder, with all subcontents\n     will be deleted!!!\n\nPress vol+/- to cancel\n", "Press power to contine...", 3)){
                clearscreen();
                gfx_printf("\nDeleting folder, please wait...\n");
                if ((res = del_recursive(currentpath))){
                    message(COLOR_RED, "Error during del_recursive()! %d", res);
                }
                writecurpath(getprevloc(currentpath));
                return readfolder(currentpath) + 1;
            }
            break;
        case COPYFOLDER:
            writeclipboard(currentpath, false, true);
            break;
    }

    return 0;
}

void fileexplorer(const char *startpath){
    int amount, res, tempint;
    bool breakfree = false;

    if (!strcmp(rootpath, "emmc:/") && !strcmp(startpath, "emmc:/"))
        clipboardhelper = 0;

    strcpy(rootpath, startpath);
    writecurpath(startpath);
    amount = readfolder(currentpath);

    if (strcmp(rootpath, "emmc:/"))
        explfilemenu[5].property = 1;
    else
        explfilemenu[5].property = -1;

    while (1){
        res = makefilemenu(fileobjects, amount, currentpath);
        if (res < 1){
            switch (res){
                case -2:
                    if (!strcmp(rootpath, currentpath))
                        breakfree = true;
                    else {
                        writecurpath(getprevloc(currentpath));
                        amount = readfolder(currentpath);
                    }

                    break;
                
                case -1:
                    if (clipboardhelper & ISDIR){
                        copyfolder(clipboard, currentpath);
                    }
                    else {
                        copyfile(clipboard, currentpath);
                    }
                    amount = readfolder(currentpath);
                    break;

                case 0:
                    tempint = foldermenu();

                    if (tempint < 0)
                        breakfree = true;
                    if (tempint > 0)
                        amount = tempint - 1;

                    break;
                
            }
        }
        else {
            if (fileobjects[res - 1].property & ISDIR){
                writecurpath(getnextloc(currentpath, fileobjects[res - 1].name));
                amount = readfolder(currentpath);
            }
            else {
                if ((tempint = filemenu(fileobjects[res - 1])))
                    amount = tempint - 1;
            }
        }

        if (breakfree)
            break;
    }
}