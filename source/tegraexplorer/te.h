#pragma once
#include "../utils/types.h"

#define MAINMENU_AMOUNT 7
#define CREDITS_MESSAGE "\nTegraexplorer, made by:\nSuch Meme, Many Skill\n\nProject based on:\nLockpick_RCM\nHekate\n\nCool people:\nshchmue\ndennthecafebabe\nDax"

typedef struct _menu_item {
    char name[50];
    u32 color;
    short internal_function;
    short property;
} menu_item;

enum mainmenu_return {
    SD_CARD = 1,
    EMMC_SYS,
    EMMC_USR,
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
    DUMPFIRMWARE
};

enum formatmenu_return {
    FORMAT_EMUMMC = 0,
    FORMAT_ALL_FAT32
};

//menu_item mainmenu[MAINMENU_AMOUNT];

void te_main();