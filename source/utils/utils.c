#include "utils.h"
#include <string.h>
#include <utils/types.h>
#include <mem/heap.h>

char *CpyStr(const char* in){
    int len = strlen(in);
    char *out = malloc(len + 1);
    out[len] = 0;
    memcpy(out, in, len);
    return out;
}

void MaskIn(char *mod, u32 bitstream, char mask){
    u32 len = strlen(mod);
    for (int i = 0; i < len; i++){
        if (!(bitstream & 1))
            *mod = mask;
        
        bitstream >>= 1;
        mod++;
    }
}

// non-zero is yes, zero is no
bool StrEndsWith(char *begin, char *end){
    begin = strrchr(begin, *end);
    if (begin != NULL)
        return !strcmp(begin, end);

    return 0;
}