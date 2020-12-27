#pragma once
#include <utils/types.h>
#include <utils/list.h>
#include "../err.h"

enum {
    MMC_CONN_None = 0,
    MMC_CONN_EMMC,
    MMC_CONN_EMUMMC
};

int connectMMC(u8 mmcType);
ErrCode_t mountMMCPart(const char *partition);
void SetKeySlots();
void unmountMMCPart();
link_t *GetCurGPT();