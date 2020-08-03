#include "keys.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../sec/se.h"
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../../config/ini.h"
#include "../common/common.h"
#include <string.h>

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

u8 *GetKey(const char *keypath, const char *keyName){
    LIST_INIT(inilist);
    char *key;
    u8 *hex = NULL;

    if (!ini_parse(&inilist, keypath, false)){
        gfx_errDisplay("GetKey", ERR_INI_PARSE_FAIL, 1);
        goto out_free;
    }

    if ((key = getKey(keyName, &inilist)) == NULL){
        gfx_errDisplay("GetKey", ERR_INI_PARSE_FAIL, 2);
        goto out_free;
    }

    hex = getHex(key);

out_free:;
    list_empty(&inilist);
    return hex;
}