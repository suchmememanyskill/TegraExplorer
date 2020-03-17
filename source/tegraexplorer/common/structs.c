#include "common.h"
#include "types.h"

menu_entry mainmenu_main[] = {
    {"[SD:/] SD CARD\n", COLOR_GREEN, ISMENU},
    {"[SYSTEM:/] EMMC", COLOR_ORANGE, ISMENU},
    {"[USER:/] EMMC", COLOR_ORANGE, ISMENU},
    {"[SAFE:/] EMMC", COLOR_ORANGE, ISMENU},
    {"\n[SYSTEM:/] EMUMMC", COLOR_BLUE, ISMENU},
    {"[USER:/] EMUMMC", COLOR_BLUE, ISMENU},
    {"[SAFE:/] EMUMMC", COLOR_BLUE, ISMENU},
    {"\nMount/Unmount SD", COLOR_WHITE, ISMENU},
    {"Tools", COLOR_VIOLET, ISMENU},
    {"SD format", COLOR_VIOLET, ISMENU},
    {"\nCredits", COLOR_WHITE, ISMENU},
    {"Exit", COLOR_WHITE, ISMENU}
};

menu_entry mainmenu_shutdown[] = {
    {"Back", COLOR_WHITE, ISMENU},
    {"\nReboot to RCM", COLOR_VIOLET, ISMENU},
    {"Reboot normally", COLOR_ORANGE, ISMENU},
    {"Power off\n", COLOR_BLUE, ISMENU},
    {"Reboot to Hekate", COLOR_GREEN, ISMENU},
    {"Reboot to Atmosphere", COLOR_GREEN, ISMENU}
};

menu_entry mainmenu_tools[] = {
    {"Back", COLOR_WHITE, ISMENU},
    {"\nDisplay Console Info", COLOR_GREEN, ISMENU},
    {"Display GPIO pins", COLOR_VIOLET, ISMENU},
    {"Dump Firmware", COLOR_BLUE, ISMENU},
    {"Dump User Saves", COLOR_YELLOW, ISMENU},
    {"[DEBUG] Dump bis", COLOR_RED, ISMENU},
    {"[DEBUG] Restore bis", COLOR_RED, ISMENU}
};

menu_entry mainmenu_format[] = {
    {"Back\n", COLOR_WHITE, ISMENU},
    {"Format entire SD to FAT32", COLOR_RED, ISMENU},
    {"Format for EmuMMC setup (FAT32/RAW)", COLOR_RED, ISMENU}
};

menu_entry utils_mmcChoice[] = {
    {"Back\n", COLOR_WHITE, ISMENU},
    {"SysMMC", COLOR_ORANGE, ISMENU},
    {"EmuMMC", COLOR_BLUE, ISMENU}
};