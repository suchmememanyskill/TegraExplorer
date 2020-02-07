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
#include "script.h"

fs_entry *fileobjects;
char rootpath[10] = "";
char *currentpath = "";
char *clipboard = "";
u8 clipboardhelper = 0;
extern const char sizevalues[4][3];
extern int launch_payload(char *path);

menu_item explfilemenu[11] = {
    {"-- File Menu --", COLOR_BLUE, -1, 0},
    {"FILE", COLOR_GREEN, -1, 0},
    {"\nSIZE", COLOR_VIOLET, -1, 0},
    {"ATTRIB", COLOR_VIOLET, -1, 0},
    {"\n\n\nBack", COLOR_WHITE, -1, 1},
    {"\nCopy to clipboard", COLOR_BLUE, COPY, 1},
    {"Move to clipboard", COLOR_BLUE, MOVE, 1},
    {"\nDelete file\n", COLOR_RED, DELETE, 1},
    {"Launch Payload", COLOR_ORANGE, PAYLOAD, 1},
    {"Launch Script", COLOR_YELLOW, SCRIPT, 1},
    {"View Hex", COLOR_GREEN, HEXVIEW, 1}
};

menu_item explfoldermenu[6] = {
    {"-- Folder Menu --\n", COLOR_BLUE, -1, 0},
    {"ATTRIB", COLOR_VIOLET, -1, 0},
    {"\n\nBack", COLOR_WHITE, -1, 1},
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

char *getnextloc(const char *current, const char *add){
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

int getfileobjamount(){
    int amount = 0;

    while (fileobjects[amount].name != NULL)
        amount++;

    return amount;
}

fs_entry getfileobj(int spot){
    return fileobjects[spot];
}

void copyfile(const char *path, const char *outfolder){
    char *filename = strrchr(path, '/') + 1;
    char *outstring;
    int res;

    clearscreen();
    makestring(getnextloc(outfolder, filename), &outstring);

    gfx_printf("Note:\nTo stop the transfer hold Vol-\n\n%s\nProgress: ", outstring);

    if (!strcmp(path, outstring)){
        message(COLOR_RED, "\nIn and out are the same!");
    }
    else if (clipboardhelper & OPERATIONMOVE){
        if (strcmp(rootpath, "emmc:/")){
            f_rename(path, outstring);
            readfolder(currentpath);
        }       
        else
            message(COLOR_RED, "\nMoving in emummc is not allowed!");
    }
    
    else if (clipboardhelper & OPERATIONCOPY) {
        res = copy(path, outstring, true, true);
        if (res){
            gfx_printf("\n\n%kSomething went wrong while copying!\n\nErrcode: %d%k", COLOR_RED, res, COLOR_WHITE);
            btn_wait();
        }
        readfolder(currentpath);
    }

    else {
        message(COLOR_RED, "\nClipboard is empty!");
    }

    free (outstring);
    clipboardhelper = 0;
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
        readfolder(currentpath);
        return 0;
    }
    else
        return -1;
}

void copyfolder(char *in, char *out){
    bool fatalerror = false;
    int res;

    if (!strcmp(in, rootpath)){
        message(COLOR_RED, "\nIn is root\nAborting!");
        fatalerror = true;
    }
    
    if (strstr(out, in) != NULL  && !fatalerror){
        message(COLOR_RED, "\nOut is a part of in!\nAborting!");
        fatalerror = true;
    }

    if (!strcmp(in, out) && !fatalerror){
        message(COLOR_RED, "\nIn is the same as out!\nAborting!");
        fatalerror = true;
    }

    if (!fatalerror){
        clearscreen();
        gfx_printf("\nCopying folder, please wait\n");
        if ((res = copy_recursive(in, out)))
            message(COLOR_RED, "copy_recursive() failed!\nErrcode %d", res);
        readfolder(currentpath);
    }
    clipboardhelper = 0;
}

int filemenu(fs_entry file){
    int temp;
    FILINFO attribs;
    strlcpy(explfilemenu[1].name, file.name, 43);
            
    for (temp = 4; temp < 8; temp++)
        if ((file.property & (1 << temp)))
            break;

    sprintf(explfilemenu[2].name, "\nSize: %d %s", file.size, sizevalues[temp - 4]);

    if (f_stat(getnextloc(currentpath, file.name), &attribs))
        explfilemenu[3].property = -1;
    else {
        explfilemenu[3].property = 0;
        sprintf(explfilemenu[3].name, "Attribs: %c%c%c%c",
        (attribs.fattrib & AM_RDO) ? 'R' : '-',
        (attribs.fattrib & AM_SYS) ? 'S' : '-',
        (attribs.fattrib & AM_HID) ? 'H' : '-',
        (attribs.fattrib & AM_ARC) ? 'A' : '-');
    }

    if (strstr(file.name, ".bin") != NULL && file.size & ISKB){
        explfilemenu[8].property = 1;
    }
    else
        explfilemenu[8].property = -1;

    if (strstr(file.name, ".tegrascript") != NULL)
        explfilemenu[9].property = 1;
    else
        explfilemenu[9].property = -1;

    temp = makemenu(explfilemenu, 11);

    switch (temp){
        case COPY:
            writeclipboard(getnextloc(currentpath, file.name), false, false);
            break;
        case MOVE:
            writeclipboard(getnextloc(currentpath, file.name), true, false);
            break;
        case DELETE:
            delfile(getnextloc(currentpath, file.name), file.name);
            break;
        case PAYLOAD:
            launch_payload(getnextloc(currentpath, file.name));
            break;
        case SCRIPT:
            ParseScript(getnextloc(currentpath, file.name));
            break;
        case HEXVIEW:
            viewbytes(getnextloc(currentpath, file.name));
            break;
    }

    return 0;
}

int foldermenu(){
    int res;
    FILINFO attribs;

    if (!strcmp(rootpath, currentpath)){
        explfoldermenu[4].property = -1;
        explfoldermenu[5].property = -1;
    }
    else {
        explfoldermenu[4].property = 1;
        explfoldermenu[5].property = 1;
    }
    
    if (f_stat(currentpath, &attribs))
        explfoldermenu[1].property = -1;
    else {
        explfoldermenu[1].property = 0;
        sprintf(explfoldermenu[1].name, "Attribs: %c%c%c%c",
        (attribs.fattrib & AM_RDO) ? 'R' : '-',
        (attribs.fattrib & AM_SYS) ? 'S' : '-',
        (attribs.fattrib & AM_HID) ? 'H' : '-',
        (attribs.fattrib & AM_ARC) ? 'A' : '-');
    }

    res = makemenu(explfoldermenu, 6);

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
                readfolder(currentpath);
            }
            break;
        case COPYFOLDER:
            writeclipboard(currentpath, false, true);
            break;
    }

    return 0;
}

void fileexplorer(const char *startpath){
    int res, tempint;
    bool breakfree = false;

    if (!strcmp(rootpath, "emmc:/") && !strcmp(startpath, "emmc:/"))
        clipboardhelper = 0;

    strcpy(rootpath, startpath);
    writecurpath(startpath);
    readfolder(currentpath);

    if (strcmp(rootpath, "emmc:/"))
        explfilemenu[6].property = 1;
    else
        explfilemenu[6].property = -1;

    while (1){
        res = makefilemenu(fileobjects, getfileobjamount(), currentpath);
        if (res < 1){
            switch (res){
                case -2:
                    if (!strcmp(rootpath, currentpath))
                        breakfree = true;
                    else {
                        writecurpath(getprevloc(currentpath));
                        readfolder(currentpath);
                    }

                    break;
                
                case -1:
                    if (clipboardhelper & ISDIR)
                        copyfolder(clipboard, currentpath);
                    else
                        copyfile(clipboard, currentpath);
                    break;

                case 0:
                    tempint = foldermenu();

                    if (tempint == -1)
                        breakfree = true;

                    break;
                
            }
        }
        else {
            if (fileobjects[res - 1].property & ISDIR){
                writecurpath(getnextloc(currentpath, fileobjects[res - 1].name));
                readfolder(currentpath);
            }
            else {
                filemenu(fileobjects[res - 1]);
            }
        }

        if (breakfree)
            break;
    }
}