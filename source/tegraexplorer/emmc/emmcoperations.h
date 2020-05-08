#pragma once
#include "../../utils/types.h"
#include "../../storage/nx_emmc.h"

int emmcDumpSpecific(char *part, char *path);
int emmcDumpBoot(char *basePath);
int mmcFlashFile(char *path, short mmcType, bool warnings);

int emmcDumpPart(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part);
int emmcRestorePart(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part, bool warnings);