#pragma once
#include "types.h"

extern const char *gfx_file_size_names[];
extern const char *menu_sd_states[];
extern const char *emmc_fs_entries[];
extern const char *utils_err_codes[];

enum mainmenu_main_return {
    MAIN_SDCARD = 0,
    MAIN_EMMC_SAF,
    MAIN_EMMC_SYS,
    MAIN_EMMC_USR,
    MAIN_EMUMMC_SAF,
    MAIN_EMUMMC_SYS,
    MAIN_EMUMMC_USR,
    MAIN_MOUNT_SD,
    MAIN_TOOLS,
    MAIN_SD_FORMAT,
    MAIN_CREDITS,
    MAIN_EXIT
};

extern menu_entry mainmenu_main[];

enum mainmenu_shutdown_return {
    SHUTDOWN_REBOOT_RCM = 1,
    SHUTDOWN_REBOOT_NORMAL,
    SHUTDOWN_POWER_OFF,
    SHUTDOWN_HEKATE,
    SHUTDOWN_AMS
};

extern menu_entry mainmenu_shutdown[];

enum mainmenu_tools_return {
    TOOLS_DISPLAY_INFO = 1,
    TOOLS_DISPLAY_GPIO,
    TOOLS_DUMPFIRMWARE,
    TOOLS_DUMPUSERSAVE,
    TOOLS_DUMP_BOOT,
    TOOLS_RESTORE_BOOT
};

extern menu_entry mainmenu_tools[];

enum mainmenu_format_return {
    FORMAT_EMUMMC = 1,
    FORMAT_ALL_FAT32
};

extern menu_entry mainmenu_format[];

enum mmc_types {
    SYSMMC = 1,
    EMUMMC
};

extern menu_entry utils_mmcChoice[];