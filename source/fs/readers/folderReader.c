#include "folderReader.h"
#include <libs/fatfs/ff.h>
#include "../../utils/utils.h"
#include <mem/heap.h>

void clearFileVector(Vector_t *v){
    vecPDefArray(FSEntry_t*, entries, v);
    for (int i = 0; i < v->count; i++)
        free(entries[i].name);
    
    free(v->data);
}

Vector_t /* of type FSEntry_t */ ReadFolder(const char *path, int *res){
    Vector_t out = newVec(sizeof(FSEntry_t), 16); // we may want to prealloc with the same size as the folder
    DIR dir;
    FILINFO fno;

    if ((*res = f_opendir(&dir, path))){
        // Err!
        return out;
    }

    while (!(*res = f_readdir(&dir, &fno)) && fno.fname[0]) {
        FSEntry_t newEntry = {.optionUnion = fno.fattrib, .name = CpyStr(fno.fname)};

        if (!newEntry.isDir){
            u64 total = fno.fsize;
            u8 type = 0;
            while (total > 1024){
                total /= 1024;
                type++;
            }

            if (type > 3)
                type = 3;

            newEntry.showSize = 1;
            newEntry.size = total;
            newEntry.sizeDef = type;
        }
        vecAddElem(&out, newEntry);
    }

    f_closedir(&dir);

    return out;
}
