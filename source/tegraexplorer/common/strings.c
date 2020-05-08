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
    "DEFENITION OF INSANITY",
    "FOLDER ROOT",
    "DEST PART OF SRC",
    "PART NOT FOUND",
    "BISKEY DUMP FAILED",
    "MEM ALLOC FAILED",
    "EMMC READ FAILED",
    "EMMC WRITE FAILED",
    "FILE TOO BIG FOR DEST",
    "SD EJECTED",
    "PARSING FAILED",
    "CANNOT COPY FILE TO FS PART",
    "NO DESTINATION",
    "INI PARSE FAIL"
};
/*
const char *pkg2names[] = {
    "BCPKG2-1-Normal-Main",
    "BCPKG2-2-Normal-Sub",
    "BCPKG2-3-SafeMode-Main",
    "BCPKG2-4-SafeMode-Sub",
    "BCPKG2-5-Repair-Main",
    "BCPKG2-6-Repair-Sub"
};
*/
const char *mainmenu_credits = "\nTegraexplorer, made by:\nSuch Meme, Many Skill\n\nProject based on:\nLockpick_RCM\nHekate\n\nCool people:\nshchmue\ndennthecafebabe\nDax";
