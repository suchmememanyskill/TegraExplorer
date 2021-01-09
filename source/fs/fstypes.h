#pragma once
#include <utils/types.h>

typedef struct {
    char *name;
    union {
        struct {
            u8 readOnly:1;
            u8 hidden:1;
            u8 system:1;
            u8 volume:1;
            u8 isDir:1;
            u8 archive:1;
        };
        u8 optionUnion;
    };
    union {
        struct {
            u16 size:12;
            u16 showSize:1;
            u16 sizeDef:3;
        };
        u16 sizeUnion;
    };
} FSEntry_t;

#define newFSEntry(filename) (FSEntry_t) {.name = filename}