#include "keys.h"
#include "keyfile.h"
#include <utils/types.h>
#include <libs/fatfs/ff.h>
#include <string.h>
#include <utils/ini.h>
#include "../tegraexplorer/tconf.h"
#include <storage/nx_sd.h>
#include "../gfx/gfx.h"

#define GetHexFromChar(c) ((c & 0x0F) + (c >= 'A' ? 9 : 0))

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

void AddKey(u8 *buff, char *in, u32 len){
    if (in == NULL || strlen(in) != len * 2)
        return;
    
    for (int i = 0; i < len; i++)
        buff[i] = (u8)((GetHexFromChar(in[i * 2]) << 4) | GetHexFromChar(in[i * 2 + 1]));
}

int GetKeysFromFile(char *path){
    gfx_puts("Grabbing keys from prod.keys...");
    if (!sd_mount())
        return 1;

    LIST_INIT(iniList); // Whatever we'll just let this die in memory hell
    if (!ini_parse(&iniList, path, false))
        return 1;

    // add biskeys, mkey 0, header_key, save_mac_key
    AddKey(dumpedKeys.bis_key[0], getKey("bis_key_00", &iniList), AES_128_KEY_SIZE * 2);
    AddKey(dumpedKeys.bis_key[1], getKey("bis_key_01", &iniList), AES_128_KEY_SIZE * 2);
    AddKey(dumpedKeys.bis_key[2], getKey("bis_key_02", &iniList), AES_128_KEY_SIZE * 2);
    AddKey(dumpedKeys.master_key, getKey("master_key_00", &iniList), AES_128_KEY_SIZE);
    AddKey(dumpedKeys.header_key, getKey("header_key", &iniList), AES_128_KEY_SIZE * 2);
    AddKey(dumpedKeys.save_mac_key, getKey("save_mac_key", &iniList), AES_128_KEY_SIZE);
    
    gfx_puts(" Done");
    return 0;
}