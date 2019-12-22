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

fs_entry fileobjects[500];
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
    char *ret;
    size_t size = strlen(current) + strlen(add) + 1;
    ret = (char*) malloc (size);
    if (!strcmp(rootpath, current))
        sprintf(ret, "%s%s", current, add);
    else
        sprintf(ret, "%s/%s", current, add);

    return ret;
}

char *getprevloc(char *current){
    char *ret, *temp;
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
        message("File Opening Failed", COLOR_RED);
        return;
    }

    msleep(200);

    while (1){
        f_lseek(&in, offset * 16);

        res = f_read(&in, &print, 2048 * sizeof(u8), &size);
        if (res != FR_OK){
            message("Reading Failed", COLOR_RED);
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

int copy(const char *locin, const char *locout, bool print){
    FIL in, out;
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
            gfx_printf("%k[%d%%/100%%]%k", COLOR_GREEN, ((sizecopied * 100) / totalsize) ,COLOR_WHITE);
            gfx_con_setpos(x, y);

            i = 0;

            if (btn_read() & BTN_VOL_DOWN){
                f_unlink(locout);
                break;
            }
        }
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
            message("\nMoving in emummc is not allowed!", COLOR_RED);
    }
    
    else if (clipboardhelper & OPERATIONCOPY) {
        res = copy(path, outstring, true);
        if (res){
            gfx_printf("\n\n%kSomething went wrong while copying!\n\nErrcode: %d%k", COLOR_RED, res, COLOR_WHITE);
            btn_wait();
        }
    }

    else {
        message("\nClipboard is empty!", COLOR_RED);
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

int readfolder(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0, res;
    
    if (res = f_opendir(&dir, path)){
        char errmes[50] = "";
        sprintf(errmes, "Error during f_opendir: %d", res);
        message(errmes, COLOR_RED);
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0] && folderamount < 500){
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

void filemenu(const char *startpath){
    int amount, res, tempint;
    char temp[100];

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
            if (res == -2){
                if (!strcmp(rootpath, currentpath))
                    break;
                else {
                    writecurpath(getprevloc(currentpath));
                    amount = readfolder(currentpath);
                }
            }
            if (res == -1){
                copyfile(clipboard, currentpath);
                amount = readfolder(currentpath);
            }
                
            if (res == 0)
                break;
        }
        else {
            if (fileobjects[res - 1].property & ISDIR){
                writecurpath(getnextloc(currentpath, fileobjects[res - 1].name));
                amount = readfolder(currentpath);
            }
            else {
                strlcpy(explfilemenu[1].name, fileobjects[res - 1].name, 43);
            
                for (tempint = 4; tempint < 8; tempint++)
                    if ((fileobjects[res - 1].property & (1 << tempint)))
                        break;

                sprintf(temp, "\nSize: %d %s", fileobjects[res - 1].size, sizevalues[tempint - 4]);
                strcpy(explfilemenu[2].name, temp);

                if (strstr(fileobjects[res - 1].name, ".bin") != NULL && fileobjects[res - 1].size & ISKB){
                    explfilemenu[7].property = 1;
                }
                else
                    explfilemenu[7].property = -1;

                tempint = makemenu(explfilemenu, 9);

                switch (tempint){
                    case COPY:
                        writeclipboard(getnextloc(currentpath, fileobjects[res - 1].name), false, false);
                        break;
                    case MOVE:
                        writeclipboard(getnextloc(currentpath, fileobjects[res - 1].name), true, false);
                        break;
                    case DELETE:
                        if ((tempint = delfile(getnextloc(currentpath, fileobjects[res - 1].name), fileobjects[res - 1].name)) != -1)
                                amount = tempint;
                        break;
                    case PAYLOAD:
                        launch_payload(getnextloc(currentpath, fileobjects[res - 1].name));
                        break;
                    case HEXVIEW:
                        viewbytes(getnextloc(currentpath, fileobjects[res - 1].name));
                        break;
                }
            }
        }
    }
}