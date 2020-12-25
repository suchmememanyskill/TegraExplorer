#pragma once
#include <utils/types.h>

enum {
    LOC_None = 0,
    LOC_SD,
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
            u16 minervaEnabled:1;
            u16 curExplorerLoc:2;
            u16 heldExplorerCopyLoc:2;
            u16 explorerCopyMode:2;
            u16 currentMMCMounted:2;
        };
        u16 optionUnion;
    };
    // Add keys here
} TConf_t;

extern TConf_t TConf;

void ResetCopyParams();
void SetCopyParams(char *path, u8 mode);