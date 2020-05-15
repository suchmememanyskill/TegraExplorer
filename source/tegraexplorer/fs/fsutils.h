#pragma once
#include "../common/types.h"
#include "../../utils/types.h"

typedef struct {
    u8 version[0x10];
    u8 args;
    u32 sizes[4];
} __attribute__((__packed__)) BisFile;

#define FLIPU32(in) ((in >> 24) & 0xFF) | ((in >> 8) & 0xFF00) | ((in << 8) & 0xFF0000) | ((in << 24) & 0xFF000000)

char *fsutil_getnextloc(const char *current, const char *add);
char *fsutil_getprevloc(char *current);
bool fsutil_checkfile(char* path);
u64 fsutil_getfilesize(char *path);
int fsutil_getfolderentryamount(const char *path);
int extract_bis_file(char *path, char *outfolder);
char *fsutil_formatFileAttribs(char *path);