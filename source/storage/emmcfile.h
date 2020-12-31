#pragma once
#include "../err.h"

ErrCode_t DumpOrWriteEmmcPart(const char *path, const char *part, u8 write, u8 force);