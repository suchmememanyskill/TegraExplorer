#pragma once
#include <utils/types.h>
#include "../../utils/vector.h"

typedef struct {
    char *name;
    union {
        struct {
            u8 isDir:1;
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

Vector_t /* of type FSEntry_t */ ReadFolder(char *path);