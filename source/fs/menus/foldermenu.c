#include "foldermenu.h"
#include "../../err.h"
#include "../../gfx/menu.h"
#include "../../gfx/gfxutils.h"
#include "../fsutils.h"
#include <mem/heap.h>
#include <string.h>
#include <utils/sprintf.h>
#include "../../tegraexplorer/tconf.h"
#include "../../hid/hid.h"
#include <libs/fatfs/ff.h>
#include "../../utils/utils.h"
#include "../../keys/nca.h"
#include "../../script/lexer.h"
#include "../../script/parser.h"
#include "../../script/variables.h"
#include <storage/nx_sd.h>
#include "../fscopy.h"

MenuEntry_t FolderMenuEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "-- Folder menu --"},
    {.optionUnion = COLORTORGB(COLOR_GREEN) | SKIPBIT}, // For the file name and size
    {.optionUnion = COLORTORGB(COLOR_VIOLET) | SKIPBIT}, // For the file Attribs
    {.optionUnion = HIDEBIT},
    {.optionUnion = COLORTORGB(COLOR_WHITE), .name = "<- Back"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "\nCopy to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Move to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Rename current folder\n"},
    {.optionUnion = COLORTORGB(COLOR_RED), .name = "Delete current folder"},
    {.optionUnion = COLORTORGB(COLOR_GREEN), .name = "\nCreate folder"}
};

int UnimplementedFolderException(const char *path){
    DrawError(newErrCode(TE_ERR_UNIMPLEMENTED));
    return 0;
}

int FolderCopyClipboard(const char *path){
    SetCopyParams(path, CMODE_CopyFolder);
    return 0;
}

int FolderMoveClipboard(const char *path){
    SetCopyParams(path, CMODE_MoveFolder);
    return 0;
}

int DeleteFolder(const char *path){
    gfx_con_setpos(384 + 16, 200 + 16 + 10 * 16);
    SETCOLOR(COLOR_RED, COLOR_DARKGREY);
    gfx_printf("Are you sure?        ");

    WaitFor(1000);
    if (MakeYesNoHorzMenu(3, COLOR_DARKGREY)){
        gfx_clearscreen();
        SETCOLOR(COLOR_RED, COLOR_DEFAULT);
        gfx_printf("\nDeleting... ");
        ErrCode_t err = FolderDelete(path);
        if (err.err){
            DrawError(err);
        }
        else return 1;
    }
    return 0;
}

int RenameFolder(const char *path){
    char *prev = EscapeFolder(path);
    gfx_clearscreen();

    char *renameTo = ShowKeyboard(strrchr(path, '/') + 1, false);
    if (renameTo == NULL || !(*renameTo)) // smol memory leak but eh
        return 0;
    
    char *dst = CombinePaths(prev, renameTo);

    int res = f_rename(path, dst);
    if (res){
        DrawError(newErrCode(res));
    }

    free(prev);
    free(dst);
    free(renameTo);
    return 1;
}

int CreateFolder(const char *path){
    gfx_clearscreen();

    char *create = ShowKeyboard("New Folder", true);
    if (create == NULL || !(*create)) // smol memory leak but eh
        return 0;
    
    char *dst = CombinePaths(path, create);
    f_mkdir(dst);

    free(dst);
    free(create);
    return 0;
}

folderMenuPath FolderMenuPaths[] = {
    FolderCopyClipboard,
    FolderMoveClipboard,
    RenameFolder,
    DeleteFolder,
    CreateFolder
};

int FolderMenu(const char *path){
    FSEntry_t file = GetFileInfo(path);
    FolderMenuEntries[1].name = file.name;

    char attribs[16];
    char *attribList = GetFileAttribs(file);
    s_printf(attribs, "Attribs:%s\n", attribList);
    free(attribList);
    FolderMenuEntries[2].name = attribs;

    // If root, disable all options other than create folder!
    int isRoot = !strcmp(file.name, "Root");
    FolderMenuEntries[2].hide = isRoot;
    for (int i = 5; i <= 8; i++)
        FolderMenuEntries[i].hide = isRoot;
    

    Vector_t ent = vecFromArray(FolderMenuEntries, ARR_LEN(FolderMenuEntries), sizeof(MenuEntry_t));
    gfx_boxGrey(384, 200, 384 + 512, 200 + 320, 0x33);
    gfx_con_setpos(384 + 16, 200 + 16);
    int res = newMenu(&ent, 0, 30, 19, ENABLEB | ALWAYSREDRAW | USELIGHTGREY, ent.count);
    
    if (res <= 4)
        return 0;
    
    return FolderMenuPaths[res - 5](path);
} 