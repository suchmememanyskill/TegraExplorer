#include "savesign.h"
#include "keys.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../sec/se.h"
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../../config/ini.h"
#include "../common/common.h"
#include <string.h>

bool save_commit(const char *path, u8 *save_mac_key){
    FIL file;
    int res;
    u8 *buf, hash[0x20], *cmac_data, cmac[0x10];
    const u32 hashed_data_size = 0x3D00, cmac_data_size = 0x200;
    bool success = false;
    
    buf = malloc(hashed_data_size);
    cmac_data = malloc(cmac_data_size);

    if ((res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ | FA_WRITE))){
        gfx_errDisplay("save_commit", res, 1);
        goto out_free;
    }

    f_lseek(&file, 0x300);
    if ((res = f_read(&file, buf, hashed_data_size, NULL))){
        gfx_errDisplay("save_commit", res, 2);
        goto out_free;
    }

    se_calc_sha256(hash, buf, hashed_data_size);

    f_lseek(&file, 0x108);
    if ((res = f_write(&file, hash, sizeof(hash), NULL))){
        gfx_errDisplay("save_commit", res, 3);
        goto out_free;
    }

    f_lseek(&file, 0x100);
    if ((res = f_read(&file, cmac_data, cmac_data_size, NULL))){
        gfx_errDisplay("save_commit", res, 4);
        goto out_free;
    }

    se_aes_key_set(3, save_mac_key, 0x10);
    se_aes_cmac(3, cmac, 0x10, cmac_data, cmac_data_size);

    f_lseek(&file, 0);

    if ((res = f_write(&file, cmac, sizeof(cmac), NULL))){
        gfx_errDisplay("save_commit", res, 5);
        goto out_free;
    }

    success = true;

out_free:;
    free(buf);
    free(cmac_data);
    f_close(&file);
    return success;
}

bool save_sign(const char *keypath, const char *savepath){
    u8 *key = GetKey(keypath, "save_mac_key");

    if (key == NULL){
        return false;
    }

    if (!save_commit(savepath, key))
        return false;

    return true;
}