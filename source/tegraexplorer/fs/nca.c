#include "nca.h"
#include "keys.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../sec/se.h"
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../../config/ini.h"
#include "../common/common.h"
#include <string.h>

int SetHeaderKey(){
    u8 *header_key = GetKey("sd:/switch/prod.keys", "header_key");

    if (header_key == NULL)
        return -1;

    se_aes_key_set(4, header_key + 0x00, 0x10);
    se_aes_key_set(5, header_key + 0x10, 0x10);

    return 0;
}

int GetNcaType(char *ncaPath){
    FIL fp;
    u32 read_bytes = 0;

    if (f_open(&fp, ncaPath, FA_READ | FA_OPEN_EXISTING))
        return -1;

    u8 *dec_header = (u8*)malloc(0x600);

    if (f_lseek(&fp, 0x200) || f_read(&fp, dec_header, 32, &read_bytes) || read_bytes != 32){
        f_close(&fp);
        free(dec_header);
        return -1;
    }

    se_aes_xts_crypt(5,4,0,1,dec_header + 0x200, dec_header, 32, 1);

    u8 ContentType = dec_header[0x205];
    
    f_close(&fp);
    free(dec_header);
    return ContentType;
}