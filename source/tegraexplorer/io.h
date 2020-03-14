#pragma once

bool checkfile(char* path);
void viewbytes(char *path);
int copy(const char *locin, const char *locout, bool print, bool canCancel);
u64 getfilesize(char *path);
int getfolderentryamount(const char *path);
void makestring(char *in, char **out);
int del_recursive(char *path);
int copy_recursive(char *path, char *dstpath);
int dump_emmc_parts(u16 parts, u8 mmctype);

#define PART_BOOT 0x1
#define PART_PKG2 0x2

static const char *pkg2names[] = {
    "BCPKG2-1-Normal-Main",
    "BCPKG2-2-Normal-Sub",
    "BCPKG2-3-SafeMode-Main",
    "BCPKG2-4-SafeMode-Sub",
    "BCPKG2-5-Repair-Main",
    "BCPKG2-6-Repair-Sub"
};