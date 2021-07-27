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
#include <storage/nx_sd.h>
#include "../../storage/emummc.h"
#include "../../script/eval.h"
#include "../../script/parser.h"
#include "../../script/garbageCollector.h"


MenuEntry_t FileMenuEntries[] = {
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
    
    WaitFor(500);
    if (!MakeYesNoHorzMenu(3, COLOR_DARKGREY))
        return;

    char *thing = CombinePaths(path, entry.name);
    int res = f_unlink(thing);
    if (res)
        DrawError(newErrCode(res));
    free(thing);
}

void RunScriptString(char *str, u32 size){
    TConf.scriptCWD = "sd:/";
    gfx_clearscreen();
    ParserRet_t ret = parseScript(str, size);
    setStaticVars(&ret.staticVarHolder);
    initRuntimeVars();
    eval(ret.main.operations.data, ret.main.operations.count, 0);
    exitRuntimeVars();
    exitStaticVars(&ret.staticVarHolder);
    exitFunction(ret.main.operations.data, ret.main.operations.count);
    vecFree(ret.staticVarHolder);
    vecFree(ret.main.operations);
}

void RunScript(char *path, FSEntry_t entry){
    char *thing = CombinePaths(path, entry.name);
    u32 size;
    char *script = sd_file_read(thing, &size);
    TConf.scriptCWD = thing;

    if (!script)
        return;

    if (((entry.size >= 16 && entry.sizeDef == 1) || entry.sizeDef >= 2) && !TConf.minervaEnabled)
        return;

    gfx_clearscreen();

    ParserRet_t ret = parseScript(script, size);
    free(script);
    setStaticVars(&ret.staticVarHolder);
    initRuntimeVars();
    Variable_t* res = eval(ret.main.operations.data, ret.main.operations.count, 1);
    exitRuntimeVars();
    exitStaticVars(&ret.staticVarHolder);
    exitFunction(ret.main.operations.data, ret.main.operations.count);
    vecFree(ret.staticVarHolder);
    vecFree(ret.main.operations);
    free(thing);
}

void RenameFile(char *path, FSEntry_t entry){
    gfx_clearscreen();
    char *renameTo = ShowKeyboard(entry.name, false);
    if (renameTo == NULL || !(*renameTo)) // smol memory leak but eh
        return;
    
    char *src = CombinePaths(path, entry.name);
    char *dst = CombinePaths(path, renameTo);

    int res = f_rename(src, dst);
    if (res){
        DrawError(newErrCode(res));
    }

    free(src);
    free(dst);
    free(renameTo);
}

// This is from the original TE and it's bad but uhh i'm too lazy to fix it
void HexView(char *path, FSEntry_t entry){
    char *filePath = CombinePaths(path, entry.name);

    FIL in;
    u8 *print;
    u32 size;
    QWORD offset = 0;
    int res;
    Input_t *input = hidRead();

    while (input->buttons & (BtnPow | JoyB))
        hidRead();

    gfx_clearscreen();
    print = calloc(2048, 1);

    if ((res = f_open(&in, filePath, FA_READ | FA_OPEN_EXISTING))){
        DrawError(newErrCode(res));
        return;
    }

    while (1){
        f_lseek(&in, offset * 32);

        if ((res = f_read(&in, print, 2048, &size))){
            DrawError(newErrCode(res));
            return;
        }

        gfx_con_setpos(0, 31);
        gfx_hexdump(offset * 32, print, ((size + 31) / 32) * 32);

        input = hidRead();

        if (!(input->buttons))
            input = hidWait();

        if (input->down && 2048 == size)
            offset += 2;
        if (input->up && offset > 0)
            offset -= 2;
        if (input->buttons & (BtnPow | JoyB))
            break;
    }
    f_close(&in);
    free(print);
    free(filePath);
}

fileMenuPath FileMenuPaths[] = {
    CopyClipboard,
    MoveClipboard,
    RenameFile,
    DeleteFile,
    HexView,
    LaunchPayload,
    RunScript
};

void FileMenu(char *path, FSEntry_t entry){
    FileMenuEntries[1].name = entry.name;
    FileMenuEntries[0].sizeUnion = entry.sizeUnion;
    char attribs[16];
    char *attribList = GetFileAttribs(entry);
    s_printf(attribs, "Attribs:%s\n", attribList);
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