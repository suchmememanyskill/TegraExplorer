#include "utils.h"
#include <string.h>
#include <mem/heap.h>

char *CpyStr(const char* in){
    int len = strlen(in);
    char *out = malloc(len + 1);
    out[len] = 0;
    memcpy(out, in, len);
    return out;
}