#pragma once
#include <utils/types.h>
#include "../keys/keys.h"

enum {
    LOC_None = 0,
    LOC_SD,
    LOC_EMMC,
    LOC_EMUMMC
};

enum {
    CMODE_None = 0,
    CMODE_Copy,
    CMODE_Move,
    CMODE_CopyFolder,
    CMODE_MoveFolder
};

typedef struct {
    u32 FSBuffSize;
    char *srcCopy;
    union {
        struct {
            u16 minervaEnabled:1;
            u16 keysDumped:1;
            u16 curExplorerLoc:2;
            u16 heldExplorerCopyLoc:2;
            u16 explorerCopyMode:4;
            u16 currentMMCConnected:2;
            u16 connectedMMCMounted:1;
        };
        u16 optionUnion;
    };
    const char *pkg1ID;
    u8 pkg1ver;
    char *scriptCWD;
} TConf_t;

extern TConf_t TConf;

void ResetCopyParams();
void SetCopyParams(const char *path, u8 mode);