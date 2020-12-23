#include "folderReader.h"
#include <libs/fatfs/ff.h>
#include "../../utils/utils.h"

Vector_t /* of type FSEntry_t */ ReadFolder(char *path){
    Vector_t out = newVec(sizeof(FSEntry_t), 16); // we may want to prealloc with the same size as the folder
    DIR dir;
    FILINFO fno;
    int res;

    if ((res = f_opendir(&dir, path))){
        // Err!
        return out;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]) {
        FSEntry_t newEntry = {.isDir = (fno.fattrib & AM_DIR) ? 1 : 0, .name = CpyStr(fno.fname)};
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

    return out;
}
