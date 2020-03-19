#include "fsutils.h"
#include <string.h>
#include "../../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../../utils/types.h"
#include "../../storage/emummc.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../mem/heap.h"
#include "../../storage/nx_emmc.h"
#include "../common/types.h"
#include "../utils/utils.h"

u32 DecodeInt(u8* data) {
    u32 out = 0;

    for (int i = 0; i < 4; i++) {
        out |= (data[i] << ((3 - i) * 8));
    }

    return out;
}

void copy_fil_size(FIL* in_src, FIL* out_src, int size_src){
    u8* buff;
    buff = calloc(16384, sizeof(u8));

    int size = size_src;
    int copysize;

    while (size > 0){
        copysize = MIN(16834, size);
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

int extract_bis_file(char *path, char *outfolder){
    FIL in;
    int res;
    u8 version[0x10];
    u8 args;
    u8 temp[0x4];
    u32 filesizes[4];

    if ((res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING))){
        gfx_errDisplay("extract_bis_file", res, 0);
        return -1;
    }

    f_read(&in, version, 0x10, NULL);
    f_read(&in, &args, 1, NULL);

    for (int i = 0; i < 4; i++){
        f_read(&in, temp, 4, NULL);
        filesizes[i] = DecodeInt(temp);
    }

    gfx_printf("Version: %s\n\n", version);

    gfx_printf("\nExtracting BOOT0\n");
    if (args & BOOT0_ARG)
        gen_part(filesizes[0], &in, fsutil_getnextloc(outfolder, "BOOT0.bin"));

    gfx_printf("Extracting BOOT1\n");
    if (args & BOOT1_ARG)
        gen_part(filesizes[1], &in, fsutil_getnextloc(outfolder, "BOOT1.bin"));

    gfx_printf("Extracting BCPKG2_1\n");
    if (args & BCPKG2_1_ARG)
        gen_part(filesizes[2], &in, fsutil_getnextloc(outfolder, "BCPKG2-1-Normal-Main"));

    gfx_printf("Extracting BCPKG2_3\n");
    if (args & BCPKG2_3_ARG)
        gen_part(filesizes[3], &in, fsutil_getnextloc(outfolder, "BCPKG2-3-SafeMode-Main"));

    gfx_printf("\n\nDone!\n");

    f_close(&in);
    return 0;   
}