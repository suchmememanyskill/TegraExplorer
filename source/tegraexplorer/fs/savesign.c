#include "savesign.h"
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


char *getKey(const char *search, link_t *inilist){
    LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, inilist, link){
        if (ini_sec->type == INI_CHOICE){
            LIST_FOREACH_ENTRY(ini_kv_t, kv, &ini_sec->kvs, link)
	    	{
                if (!strcmp(search, kv->key))
                    return kv->val;
	    	}
        }   
    }
    return NULL;
}

u8 getHexSingle(const char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
}

u8 *getHex(const char *in){
    u32 len = strlen(in), count = 0;
    u8 *out = calloc(len / 2, sizeof(u8));

    for (u32 i = 0; i < len; i += 2){
        out[count++] = (u8)(getHexSingle(in[i]) << 4) | (getHexSingle(in[i + 1]));
    }

    return out;
}

bool save_sign(const char *keypath, const char *savepath){
    LIST_INIT(inilist);
    char *key;
    u8 *hex;
    bool success = false;

    if (!ini_parse(&inilist, keypath, false)){
        gfx_errDisplay("save_sign", ERR_INI_PARSE_FAIL, 1);
        goto out_free;
    }

    if ((key = getKey("save_mac_key", &inilist)) == NULL){
        gfx_errDisplay("save_sign", ERR_INI_PARSE_FAIL, 2);
        goto out_free;
    }

    hex = getHex(key);

    if (!save_commit(savepath, hex))
        goto out_free;

    success = true;

out_free:;
    list_empty(&inilist);
    return success;
}