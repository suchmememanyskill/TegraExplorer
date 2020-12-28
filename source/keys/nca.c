#include "nca.h"
#include "keys.h"
#include <libs/fatfs/ff.h>
#include <sec/se.h>
#include <mem/heap.h>

// Thanks switchbrew https://switchbrew.org/wiki/NCA_Format
// This function is hacky, should change it but am lazy
int GetNcaType(char *path){
    FIL fp;
    u32 read_bytes = 0;

    if (f_open(&fp, path, FA_READ | FA_OPEN_EXISTING))
        return -1;

    u8 *dec_header = (u8*)malloc(0x400);

    if (f_lseek(&fp, 0x200) || f_read(&fp, dec_header, 32, &read_bytes) || read_bytes != 32){
        f_close(&fp);
        free(dec_header);
        return -1;
    }

    se_aes_xts_crypt(7,6,0,1, dec_header + 0x200, dec_header, 32, 1);

    u8 ContentType = dec_header[0x205];
    
    f_close(&fp);
    free(dec_header);
    return ContentType;
}