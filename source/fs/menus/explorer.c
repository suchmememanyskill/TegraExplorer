#include "explorer.h"
#include "../../utils/vector.h"
#include "../readers/folderReader.h"
#include "../../gfx/menu.h"
#include "../fsutils.h"
#include "../../gfx/gfx.h"
#include "../../gfx/gfxutils.h"
#include "../../utils/utils.h"
#include "filemenu.h"
#include <string.h>
#include <mem/heap.h>
#include "../../tegraexplorer/tconf.h"
#include "../../err.h"
#include "../fscopy.h"
#include <libs/fatfs/ff.h>
#include "../../hid/hid.h"

MenuEntry_t topEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_GREEN) | SKIPBIT},
    {.optionUnion = COLORTORGB(COLOR_ORANGE)},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Clipboard -> Current folder"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Current folder options"}
};

MenuEntry_t MakeMenuOutFSEntry(FSEntry_t entry){
    MenuEntry_t out = {.name = entry.name, .sizeUnion = entry.sizeUnion};
    out.optionUnion = (entry.isDir) ? COLORTORGB(COLOR_WHITE) : COLORTORGB(COLOR_VIOLET);
    out.icon = (entry.isDir) ? 127 : 128;

    return out;
}



void FileExplorer(char *path){
    char *storedPath = CpyStr(path);
    int res = 0;

    while (1){
        topEntries[2].optionUnion = (TConf.explorerCopyMode != CMODE_None) ? (COLORTORGB(COLOR_ORANGE)) : (COLORTORGB(COLOR_GREY) | SKIPBIT);
        topEntries[1].name = (!strcmp(storedPath, path)) ? "<- Exit explorer" : "<- Folder back";

        gfx_clearscreen();
        gfx_printf("Loading...\r");

        int readRes = 0;
        Vector_t fileVec = ReadFolder(storedPath, &readRes);
        if (readRes){
            clearFileVector(&fileVec);
            DrawError(newErrCode(readRes));
            return;
        }

        vecDefArray(FSEntry_t*, fsEntries, fileVec);
        
        topEntries[0].name = storedPath;
        Vector_t entries = newVec(sizeof(MenuEntry_t), fileVec.count + ARR_LEN(topEntries));
        entries.count = ARR_LEN(topEntries);
        memcpy(entries.data, topEntries, sizeof(MenuEntry_t) * ARR_LEN(topEntries));

        for (int i = 0; i < fileVec.count; i++){
            MenuEntry_t a = MakeMenuOutFSEntry(fsEntries[i]);
            vecAddElem(&entries, a);
        }

        gfx_con_setpos(144, 24);
        gfx_boxGrey(0, 16, 160, 31, 0x1B);

        if (res >= fileVec.count + ARR_LEN(topEntries))
            res = 0;

        res = newMenu(&entries, res, 60, 42, ENABLEB | ENABLEPAGECOUNT, (int)fileVec.count);

        char *oldPath = storedPath;

        if (res == 2){
            ErrCode_t res;
            char *filename = CpyStr(strrchr(TConf.srcCopy, '/') + 1);
            char *dst = CombinePaths(storedPath, filename);

            if (!strcmp(TConf.srcCopy, dst))
                res = newErrCode(TE_ERR_SAME_LOC);
            else {
                if (TConf.explorerCopyMode == CMODE_Move){
                    if ((res.err = f_rename(TConf.srcCopy, dst)))
                        res = newErrCode(res.err);
                }
                else {
                    gfx_clearscreen();
                    RESETCOLOR;
                    gfx_printf("Hold vol+/- to cancel\nCopying %s... ", filename);
                    res = FileCopy(TConf.srcCopy, dst, COPY_MODE_CANCEL | COPY_MODE_PRINT);
                }
            }
            
            DrawError(res);
            free(dst);
            free(filename);
            ResetCopyParams();
        }
        else if (res < ARR_LEN(topEntries)) {
            if (!strcmp(storedPath, path)){
                clearFileVector(&fileVec);
                return;
            }

            storedPath = EscapeFolder(oldPath);
            free(oldPath);
            res = 0;
        }
        else if (fsEntries[res - ARR_LEN(topEntries)].isDir) {
            storedPath = CombinePaths(storedPath, fsEntries[res - ARR_LEN(topEntries)].name);
            free(oldPath);
            res = 0;
        }
        else {
            FileMenu(storedPath, fsEntries[res - ARR_LEN(topEntries)]);
        }

        clearFileVector(&fileVec);
    }
}