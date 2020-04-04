#pragma once
#include "../../utils/types.h"
#include "../../storage/nx_emmc.h"

int dump_emmc_parts(u16 parts, u8 mmctype);
int restore_bis_using_file(char *path, u8 mmctype);
int emmcDumpSpecific(char *part, char *path);
int restore_emmc_part(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part);
int emmcDumpBoot(char *basePath);