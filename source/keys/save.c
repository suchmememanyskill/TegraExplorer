#include "save.h"
#include <utils/types.h>
#include <libs/fatfs/ff.h>
#include <sec/se.h>
#include <mem/heap.h>
#include "../err.h"

ErrCode_t saveCommit(const char *path){
    FIL file;
    int res;
    u8 *buf, hash[0x20], *cmac_data, cmac[0x10];
    const u32 hashed_data_size = 0x3D00, cmac_data_size = 0x200;
    
    buf = malloc(hashed_data_size);
    cmac_data = malloc(cmac_data_size);

    if ((res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ | FA_WRITE))){
        goto out_free;
    }

    f_lseek(&file, 0x300);
    if ((res = f_read(&file, buf, hashed_data_size, NULL))){
        goto out_free;
    }

    se_calc_sha256_oneshot(hash, buf, hashed_data_size);

    f_lseek(&file, 0x108);
    if ((res = f_write(&file, hash, sizeof(hash), NULL))){
        goto out_free;
    }

    f_lseek(&file, 0x100);
    if ((res = f_read(&file, cmac_data, cmac_data_size, NULL))){
        goto out_free;
    }

    se_aes_cmac(8, cmac, 0x10, cmac_data, cmac_data_size);

    f_lseek(&file, 0);

    if ((res = f_write(&file, cmac, sizeof(cmac), NULL))){
        goto out_free;
    }

out_free:;
    free(buf);
    free(cmac_data);
    f_close(&file);
    return newErrCode(res);
}