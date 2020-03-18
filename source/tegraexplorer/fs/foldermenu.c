#include <string.h>
#include "entrymenu.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../mem/heap.h"
#include "../gfx/menu.h"
#include "fsreader.h"
#include "../gfx/gfxutils.h"
#include "fsactions.h"
#include "fsutils.h"
#include "../../utils/sprintf.h"

extern char *currentpath;
extern char *clipboard;
extern u8 clipboardhelper;

void copyfolder(char *in, char *out){
    int res;

    res = strlen(in);
    if ((*(in + res - 1) == '/')){
        gfx_errDisplay("copyfolder", ERR_FOLDER_ROOT, 1);
    }
    else if (strstr(out, in) != NULL){
        gfx_errDisplay("copyfolder", ERR_DEST_PART_OF_SRC, 2);
    }
    else if (!strcmp(in, out)){
        gfx_errDisplay("copyfolder", ERR_SAME_LOC, 3);
    }
    else {
        gfx_clearscreen();
        gfx_printf("\nCopying folder, please wait\n");
        fsact_copy_recursive(in, out);
        fsreader_readfolder(currentpath);
    }

    clipboardhelper = 0;
}

int foldermenu(){
    int res;
    FILINFO attribs;

    if (fs_menu_folder[0].name != NULL)
        free(fs_menu_folder[0].name);

    fs_menu_folder[0].name = malloc(16);

    res = strlen(currentpath);

    SETBIT(fs_menu_folder[3].property, ISHIDE, (*(currentpath + res - 1) == '/'));
    SETBIT(fs_menu_folder[4].property, ISHIDE, (*(currentpath + res - 1) == '/'));
  
    if (f_stat(currentpath, &attribs))
        SETBIT(fs_menu_folder[0].property, ISHIDE, 1);
    else {
        SETBIT(fs_menu_folder[0].property, ISHIDE, 0);
        sprintf(fs_menu_folder[0].name, "Attribs: %c%c%c%c",
        (attribs.fattrib & AM_RDO) ? 'R' : '-',
        (attribs.fattrib & AM_SYS) ? 'S' : '-',
        (attribs.fattrib & AM_HID) ? 'H' : '-',
        (attribs.fattrib & AM_ARC) ? 'A' : '-');
    }

    res = menu_make(fs_menu_folder, 5, currentpath);

    switch (res){
        case DIR_EXITFOLDER:
            return -1;
        case DIR_COPYFOLDER:
            fsreader_writeclipboard(currentpath, OPERATIONCOPY | ISDIR);
            break;
        case DIR_DELETEFOLDER:
            gfx_clearscreen();
            gfx_printf("Do you want to delete this folder?\nThe entire folder, with all subcontents\n     will be deleted!!!\n\nPress vol+/- to cancel\n");
            if (gfx_makewaitmenu("Press power to contine...", 3)){
                gfx_clearscreen();
                gfx_printf("\nDeleting folder, please wait...\n");

                fsact_del_recursive(currentpath);

                fsreader_writecurpath(fsutil_getprevloc(currentpath));
                fsreader_readfolder(currentpath);
            }
            break;
    }

    return 0;
}