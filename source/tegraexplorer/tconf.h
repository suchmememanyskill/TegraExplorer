#pragma once
#include <utils/types.h>

enum {
    LOC_SD = 0,
    LOC_EMMC,
    LOC_EMUMMC
};

enum {
    CMODE_None = 0,
    CMODE_Copy,
    CMODE_Move
};

enum {
    M_None = 0,
    M_EMMC,
    M_EMUMMC
};

typedef struct {
    u32 FSBuffSize;
    char *srcCopy;
    union {
        struct {
            u8 minervaEnabled:1;
            u8 lastExplorerLoc:2;
            u8 explorerCopyMode:2;
            u8 currentlyMounted:2;
        };
        u8 optionUnion;
    };
    // Add keys here
} TConf_t;

extern TConf_t TConf;