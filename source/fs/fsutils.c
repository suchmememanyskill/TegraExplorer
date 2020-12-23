#include <mem/heap.h>
#include <string.h>
#include "fsutils.h"
#include "../utils/utils.h"
#include <utils/sprintf.h>
#include <libs/fatfs/ff.h>

char *CombinePaths(const char *current, const char *add){
    char *ret;

    size_t size = strlen(current) + strlen(add) + 2;
    ret = (char*) malloc (size);

    sprintf(ret, (current[strlen(current) - 1] == '/') ? "%s%s" : "%s/%s", current, add);

    return ret;
}

char *EscapeFolder(char *current){
    char *ret;
    char *temp;

    ret = CpyStr(current);
    temp = strrchr(ret, '/');

    if (*(temp - 1) == ':')
        temp++;

    *temp = '\0';

    return ret;
}

u64 GetFileSize(char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}