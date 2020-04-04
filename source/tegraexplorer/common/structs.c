#include "common.h"
#include "types.h"

menu_entry mainmenu_main[] = {
    {"[SD:/] SD CARD", COLOR_GREEN, ISMENU},
    {"[EMMC:/] EMMC", COLOR_ORANGE, ISMENU},
    {"[EMMC:/] EMUMMC", COLOR_BLUE, ISMENU},
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
    {"Dump User Saves", COLOR_YELLOW, ISMENU}
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

menu_entry fs_menu_file[] = {
    {NULL, COLOR_GREEN, ISMENU | ISSKIP},
    {NULL, COLOR_VIOLET, ISMENU | ISSKIP},
    {NULL, COLOR_VIOLET, ISMENU | ISSKIP},
    {"\n\n\nBack", COLOR_WHITE, ISMENU},
    {"\nCopy to clipboard", COLOR_BLUE, ISMENU},
    {"Move to clipboard", COLOR_BLUE, ISMENU},
    {"\nDelete file\n", COLOR_RED, ISMENU},
    {"Launch Payload", COLOR_ORANGE, ISMENU},
    {"Launch Script", COLOR_YELLOW, ISMENU},
    {"View Hex", COLOR_GREEN, ISMENU},
    {"\nExtract BIS", COLOR_YELLOW, ISMENU},
    {"Restore BIS", COLOR_RED, ISMENU}
};

menu_entry fs_menu_folder[] = {
    {NULL, COLOR_VIOLET, ISMENU | ISSKIP},
    {"\nBack", COLOR_WHITE, ISMENU},
    {"Return to main menu\n", COLOR_BLUE, ISMENU},
    {"Copy to clipboard", COLOR_VIOLET, ISMENU},
    {"Delete folder", COLOR_RED, ISMENU}
};

menu_entry fs_menu_startdir[] = {
    {"Folder -> previous folder            ", COLOR_ORANGE, ISMENU},
    {"Clipboard -> Current folder          ", COLOR_ORANGE, ISMENU},
    {"Current folder menu                  ", COLOR_ORANGE, ISMENU}
};

gpt_entry_rule gpt_fs_rules[] = {
    {"PRODINFOF", 0 | isFS},
    {"SAFE", 1 | isFS },
    {"SYSTEM", 2 | isFS},
    {"USER", 3 | isFS},
    {NULL, 0}
};

menu_entry mmcmenu_start[] = {
    {"Back", COLOR_ORANGE, ISMENU},
    {"Dump File Partitions", COLOR_ORANGE, ISMENU},
    {"Clipboard -> Partition\n", COLOR_ORANGE, ISMENU},
    {"BOOT0/1", COLOR_BLUE, isBOOT | ISMENU}
};

menu_entry mmcmenu_filemenu[] = {
    {"Part:", COLOR_ORANGE, ISSKIP | ISMENU},
    {NULL, COLOR_VIOLET, ISSKIP | ISMENU},
    {"\nBack", COLOR_WHITE, ISMENU},
    {"Dump to SD", COLOR_YELLOW, ISMENU}
};