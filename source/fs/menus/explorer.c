#include "explorer.h"
#include "../../utils/vector.h"
#include "../readers/folderReader.h"
#include "../../gfx/menu.h"
#include "../fsutils.h"
#include "../../gfx/gfx.h"
#include "../../gfx/gfxutils.h"
#include "../../utils/utils.h"
#include <string.h>
#include <mem/heap.h>

MenuEntry_t topEntries[] = {
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Back"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Option2"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Option3"}
};

MenuEntry_t MakeMenuOutFSEntry(FSEntry_t entry){
    MenuEntry_t out = {.name = entry.name, .sizeUnion = entry.sizeUnion};
    out.optionUnion = (entry.isDir) ? COLORTORGB(COLOR_WHITE) : COLORTORGB(COLOR_VIOLET);
    out.icon = (entry.isDir) ? 127 : 128;

    return out;
}

#define maxYOnScreen 35

void ExpandFileList(Vector_t *vec, void* data){
    Vector_t *fileVec = (Vector_t*)data;
    vecPDefArray(FSEntry_t*, fsEntries, fileVec);
    
    int start = vec->count - 3;
    for (int i = start; i < MIN(fileVec->count, start + maxYOnScreen); i++){
        MenuEntry_t a = MakeMenuOutFSEntry(fsEntries[i]);
        vecAddElem(vec, a);
    }
}

void clearFileVector(Vector_t *v){
    vecPDefArray(FSEntry_t*, entries, v);
    for (int i = 0; i < v->count; i++)
        free(entries[i].name);
    
    free(v->data);
}

void FileExplorer(char *path){
    char *storedPath = path;
    int res = 0;

    while (1){
        gfx_clearscreen();
        gfx_printf("Loading...\r");
        Vector_t fileVec = ReadFolder(storedPath);
        vecDefArray(FSEntry_t*, fsEntries, fileVec);
        
        Vector_t entries = newVec(sizeof(MenuEntry_t), maxYOnScreen);
        entries.count = 3;
        memcpy(entries.data, topEntries, sizeof(MenuEntry_t) * 3);

        for (int i = 0; i < fileVec.count; i++){
            MenuEntry_t a = MakeMenuOutFSEntry(fsEntries[i]);
            vecAddElem(&entries, a);
        }

        // ExpandFileList was first in NULL, but now we don't need it as we aren't loading the file size dynamically
        res = newMenu(&entries, res, 50, maxYOnScreen, NULL, &fileVec, true, fileVec.count);

        if (res < 3) {
            if (!strcmp(storedPath, path)){
                clearFileVector(&fileVec);
                return;
            }

            char *copy = CpyStr(storedPath);
            storedPath = EscapeFolder(copy);
            free(copy);
        }
        else if (fsEntries[res - 3].isDir) {
            char *copy = CpyStr(storedPath);
            storedPath = CombinePaths(copy, fsEntries[res - 3].name);
            free(copy);
        }

        res = 0;
        clearFileVector(&fileVec);
    }
}