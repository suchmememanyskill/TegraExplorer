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
#include "../../keys/nca.h"
#include "../../script/lexer.h"
#include "../../script/parser.h"
#include "../../script/variables.h"
#include <storage/nx_sd.h>

MenuEntry_t FileMenuEntries[] = {
    // Still have to think up the options
    {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "-- File menu --"},
    {.optionUnion = COLORTORGB(COLOR_GREEN) | SKIPBIT}, // For the file name and size
    {.optionUnion = COLORTORGB(COLOR_VIOLET) | SKIPBIT}, // For the file Attribs
    {.optionUnion = HIDEBIT},
    {.optionUnion = COLORTORGB(COLOR_WHITE), .name = "<- Back"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "\nCopy to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Move to clipboard"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Rename file\n"},
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
    SetCopyParams(thing, CMODE_Move);
    free(thing);
}

void DeleteFile(char *path, FSEntry_t entry){
    gfx_con_setpos(384 + 16, 200 + 16 + 10 * 16);
    SETCOLOR(COLOR_RED, COLOR_DARKGREY);
    gfx_printf("Are you sure?      ");

    if (!MakeYesNoHorzMenu(3, COLOR_DARKGREY))
        return;

    char *thing = CombinePaths(path, entry.name);
    int res = f_unlink(thing);
    if (res)
        DrawError(newErrCode(res));
    free(thing);
}

void RunScript(char *path, FSEntry_t entry){
    char *thing = CombinePaths(path, entry.name);
    u32 size;
    char *script = sd_file_read(thing, &size);
    free(thing);
    if (!script)
        return;

    gfx_clearscreen();
    scriptCtx_t ctx = createScriptCtx();
    ctx.script = runLexar(script, size);
    free(script);

    printError(mainLoop(&ctx));

    freeVariableVector(&ctx.varDict);
    lexarVectorClear(&ctx.script);
    gfx_printf("\nend of script");
    hidWait();
}

menuPaths FileMenuPaths[] = {
    CopyClipboard,
    MoveClipboard,
    UnimplementedException,
    DeleteFile,
    UnimplementedException,
    LaunchPayload,
    RunScript
};

void FileMenu(char *path, FSEntry_t entry){
    FileMenuEntries[1].name = entry.name;
    FileMenuEntries[0].sizeUnion = entry.sizeUnion;
    char attribs[16];
    char *attribList = GetFileAttribs(entry);
    sprintf(attribs, "Attribs:%s\n", attribList);
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