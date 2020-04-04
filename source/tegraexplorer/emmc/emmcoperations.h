#pragma once
#include "../../utils/types.h"

int dump_emmc_parts(u16 parts, u8 mmctype);
int restore_bis_using_file(char *path, u8 mmctype);
int dump_emmc_specific(char *part, char *path);