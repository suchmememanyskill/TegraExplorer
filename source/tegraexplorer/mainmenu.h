#pragma once
#include "../utils/types.h"

#define MAINMENU_AMOUNT 12

/*
typedef struct _menu_item {
    char name[50];
    u32 color;
    short internal_function;
    short property;
} menu_item;


enum mainmenu_return {
    SD_CARD = 1,
    EMMC_SAF,
    EMMC_SYS,
    EMMC_USR,
    EMUMMC_SAF,
    EMUMMC_SYS,
    EMUMMC_USR,
    MOUNT_SD,
    TOOLS,
    SD_FORMAT,
    CREDITS,
    EXIT
};

enum shutdownmenu_return {
    REBOOT_RCM = 1,
    REBOOT_NORMAL,
    POWER_OFF,
    HEKATE,
    AMS
};

enum toolsmenu_return {
    DISPLAY_INFO = 1,
    DISPLAY_GPIO,
    DUMPFIRMWARE,
    DUMPUSERSAVE,
    DUMP_BOOT,
    RESTORE_BOOT
};

enum formatmenu_return {
    FORMAT_EMUMMC = 0,
    FORMAT_ALL_FAT32
};
*/

void te_main();