#include "filemenu.h"
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

MenuEntry_t FileMenuEntries[] = {
    // Still have to think up the options
    {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "-- File menu --"},
    {.optionUnion = COLORTORGB(COLOR_GREEN) | SKIPBIT}, // For the file name and size
    {.optionUnion = COLORTORGB(COLOR_VIOLET) | SKIPBIT}, // For the file Attribs
    {.optionUnion = HIDEBIT},
    {.optionUnion = COLORTORGB(COLOR_WHITE), .name = "<- Back"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Copy to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Move to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Rename file"},
    {.optionUnion = COLORTORGB(COLOR_RED), .name = "Delete file"},
    {.optionUnion = COLORTORGB(COLOR_GREEN), .name = "View hex"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Launch Payload"},
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "Launch Script"},
};


void UnimplementedException(char *path, FSEntry_t entry){
    DrawError(newErrCode(TE_ERR_UNIMPLEMENTED));
}

extern int launch_payload(char *path);

void LaunchPayload(char *path, FSEntry_t entry){
    launch_payload(CombinePaths(path, entry.name));
}

void CopyClipboard(char *path, FSEntry_t entry){
    char *thing = CombinePaths(path, entry.name);
    SetCopyParams(thing, CMODE_Copy);
    free(thing);
}

void MoveClipboard(char *path, FSEntry_t entry){
    char *thing = CombinePaths(path, entry.name);
    SetCopyParams(thing, CMODE_Copy);
    free(thing);
}

MenuEntry_t DeleteEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "No"},
    {.R = 255, .name = "Yes"}
};

void DeleteFile(char *path, FSEntry_t entry){
    /*
    u8 left = 0;
    
    while (1){
        gfx_con_setpos(384 + 16, 200 + 16 + 9 * 16);
        SETCOLOR(COLOR_RED, COLOR_DARKGREY);
        gfx_printf("Are you sure?   ");

        (left) ? SETCOLOR(COLOR_DARKGREY, COLOR_RED) : SETCOLOR(COLOR_RED, COLOR_DARKGREY);
        gfx_printf("Yes");
        RESETCOLOR;
        gfx_printf("   ");
        (!left) ? SETCOLOR(COLOR_DARKGREY, COLOR_YELLOW) : SETCOLOR(COLOR_YELLOW, COLOR_DARKGREY);
        gfx_printf("No");

        Input_t *input = hidWait();

        if (input->a && left)
            break;
        else if (input->right)
            left = 0;
        else if (input->left)
            left = 1;
        else if (input->a || input->b)
            return;
    }
    */
    gfx_con_setpos(384 + 16, 200 + 16 + 8 * 16);
    SETCOLOR(COLOR_RED, COLOR_DARKGREY);
    gfx_printf("Are you sure?      ");

    if (!MakeHorizontalMenu(DeleteEntries, 2, 3, COLOR_DARKGREY))
        return;

    char *thing = CombinePaths(path, entry.name);
    int res = f_unlink(thing);
    if (res)
        DrawError(newErrCode(res));
    free(thing);
}

menuPaths FileMenuPaths[] = {
    CopyClipboard,
    MoveClipboard,
    UnimplementedException,
    DeleteFile,
    UnimplementedException,
    LaunchPayload,
    UnimplementedException
};

void FileMenu(char *path, FSEntry_t entry){
    FileMenuEntries[1].name = entry.name;
    FileMenuEntries[1].sizeUnion = entry.sizeUnion;
    char attribs[15];
    char *attribList = GetFileAttribs(entry);
    sprintf(attribs, "Attribs:%s", attribList);
    free(attribList);
    FileMenuEntries[2].name = attribs;

    FileMenuEntries[10].hide = !StrEndsWith(entry.name, ".bin");
    FileMenuEntries[11].hide = !StrEndsWith(entry.name, ".te");

    Vector_t ent = vecFromArray(FileMenuEntries, ARR_LEN(FileMenuEntries), sizeof(MenuEntry_t));
    gfx_boxGrey(384, 200, 384 + 512, 200 + 320, 0x33);
    gfx_con_setpos(384 + 16, 200 + 16);
    int res = newMenu(&ent, 0, 30, 19, ENABLEB | ALWAYSREDRAW | USELIGHTGREY, ent.count);
    
    if (res <= 4)
        return;
    
    FileMenuPaths[res - 5](path, entry);
} 