#pragma once
#include "../../utils/types.h"

int fsact_copy(const char *locin, const char *locout, u8 options);
int fsact_del_recursive(char *path);
int fsact_copy_recursive(char *path, char *dstpath);