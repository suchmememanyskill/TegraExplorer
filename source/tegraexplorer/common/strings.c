#include "common.h"

const char *gfx_file_size_names[] = {
    "B ",
    "KB",
    "MB",
    "GB"
};

const char *menu_sd_states[] = {
    "\nUnmount SD",
    "\nMount SD"
};

const char *emmc_fs_entries[] = {
    "SAFE",
    "SYSTEM",
    "USER"
};

const char *utils_err_codes[] = {
    "OK",
    "I/O ERROR",
    "NO DISK",
    "NOT READY",
    "NO FILE",
    "NO PATH",
    "PATH INVALID",
    "ACCESS DENIED",
    "ACCESS DENIED",
    "INVALID PTR",
    "PROTECTED",
    "INVALID DRIVE",
    "NO MEM",
    "NO FAT",
    "MKFS ABORT"
};

const char *utils_err_codes_te[] = { // these start at 50
    "SAME LOC",
    "DISK WRITE FAILED",
    "EMPTY CLIPBOARD",
    "DEFENITION OF INSANITY"
    "FOLDER ROOT"
    "DEST PART OF SRC"
};