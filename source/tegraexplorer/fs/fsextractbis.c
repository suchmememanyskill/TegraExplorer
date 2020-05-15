#include "fsutils.h"
#include <string.h>
#include "../../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../../utils/types.h"
#include "../../storage/emummc.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../mem/heap.h"
#include "../../storage/nx_emmc.h"
#include "../common/types.h"
#include "../utils/utils.h"
#include "fsactions.h"

void copy_fil_size(FIL* in_src, FIL* out_src, int size_src){
    u8* buff;
    buff = calloc(BUFSIZE, sizeof(u8));

    int size = size_src;
    int copysize;

    while (size > 0){
        copysize = MIN(BUFSIZE, size);
        f_read(in_src, buff, copysize, NULL);
        f_write(out_src, buff, copysize, NULL);
        size -= copysize;
    }

    free(buff);
}

void gen_part(int size, FIL* in, char *path){
    FIL out;

    f_open(&out, path, FA_WRITE | FA_CREATE_ALWAYS);
    copy_fil_size(in, &out, size);
    f_close(&out);
}

const char *filenames[] = {
    "BOOT0.bin",
    "BOOT1.bin",
    "BCPKG2-1-Normal-Main",
    "BCPKG2-3-SafeMode-Main",
    "BCPKG2-2-Normal-Sub",
    "BCPKG2-4-SafeMode-Sub"
};

int extract_bis_file(char *path, char *outfolder){
    FIL in;
    int res;
    char *tempPath;
    BisFile header;

    if ((res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING))){
        gfx_errDisplay("extract_bis_file", res, 0);
        return -1;
    }

    f_read(&in, &header, sizeof(BisFile), NULL);
    
    for (int i = 0; i < 4; i++)
        header.sizes[i] = FLIPU32(header.sizes[i]);

    gfx_printf("Version: %s\n\n", header.version);

    // Loop to actually extract stuff
    for (int i = 0; i < 4; i++){
        if (!(header.args & (BIT((7 - i)))))
            continue;

        gfx_printf("Extracting %s\n", filenames[i]);
        gen_part(header.sizes[i], &in, fsutil_getnextloc(outfolder, filenames[i]));
    }

    // Loop to copy pkg2_1->2 and pkg2_3->4
    for (int i = 4; i < 6; i++){
        if (!(header.args & BIT((9 - i))))
            continue;

        utils_copystring(fsutil_getnextloc(outfolder, filenames[i - 2]), &tempPath);
        gfx_printf("Copying %s\n", filenames[i]);
        fsact_copy(tempPath, fsutil_getnextloc(outfolder, filenames[i]), COPY_MODE_PRINT);
        RESETCOLOR;
        free(tempPath);
    }

    gfx_printf("\n\nDone!\n");

    f_close(&in);
    return 0;   
}