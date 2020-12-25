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

MenuEntry_t topEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_GREEN) | SKIPBIT},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Back"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Clipboard -> Current folder"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Current folder options"}
};

MenuEntry_t MakeMenuOutFSEntry(FSEntry_t entry){
    MenuEntry_t out = {.name = entry.name, .sizeUnion = entry.sizeUnion};
    out.optionUnion = (entry.isDir) ? COLORTORGB(COLOR_WHITE) : COLORTORGB(COLOR_VIOLET);
    out.icon = (entry.isDir) ? 127 : 128;

    return out;
}

void clearFileVector(Vector_t *v){
    vecPDefArray(FSEntry_t*, entries, v);
    for (int i = 0; i < v->count; i++)
        free(entries[i].name);
    
    free(v->data);
}

void FileExplorer(char *path){
    char *storedPath = CpyStr(path);
    int res = 0;

    while (1){
        topEntries[2].optionUnion = COLORTORGB(((TConf.explorerCopyMode != CMODE_None) ? COLOR_ORANGE : COLOR_GREY)) | SKIPBIT;

        gfx_clearscreen();
        gfx_printf("Loading...\r");
        //gfx_printf("          ");
        Vector_t fileVec = ReadFolder(storedPath);
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

        if (res < ARR_LEN(topEntries)) {
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