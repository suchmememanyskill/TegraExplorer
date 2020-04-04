#pragma once
#include "types.h"

extern const char *gfx_file_size_names[];
extern const char *menu_sd_states[];
extern const char *emmc_fs_entries[];
extern const char *utils_err_codes[];
extern const char *pkg2names[];
extern const char *mainmenu_credits;

enum utils_err_codes_te_call {
    ERR_SAME_LOC = 50,
    ERR_DISK_WRITE_FAILED,
    ERR_EMPTY_CLIPBOARD,
    ERR_FOLDER_ROOT = 54,
    ERR_DEST_PART_OF_SRC,
    ERR_PART_NOT_FOUND,
    ERR_BISKEY_DUMP_FAILED,
    ERR_MEM_ALLOC_FAILED,
    ERR_EMMC_READ_FAILED,
    ERR_EMMC_WRITE_FAILED,
    ERR_FILE_TOO_BIG_FOR_DEST,
    ERR_SD_EJECTED,
    ERR_PARSE_FAIL
};

extern const char *utils_err_codes_te[];

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
    TOOLS_DUMP_BOOT
};

extern menu_entry mainmenu_tools[];

enum mainmenu_format_return {
    FORMAT_ALL_FAT32 = 1,
    FORMAT_EMUMMC
};

extern menu_entry mainmenu_format[];

enum mmc_types {
    SYSMMC = 1,
    EMUMMC
};

extern menu_entry utils_mmcChoice[];

enum fs_menu_file_return {
    FILE_COPY = 4,
    FILE_MOVE,
    FILE_DELETE,
    FILE_PAYLOAD,
    FILE_SCRIPT,
    FILE_HEXVIEW,
    FILE_DUMPBIS,
    FILE_RESTOREBIS
};

extern menu_entry fs_menu_file[];

enum fs_menu_folder_return {
    DIR_EXITFOLDER = 2,
    DIR_COPYFOLDER,
    DIR_DELETEFOLDER
};

extern menu_entry fs_menu_folder[];

enum fs_menu_startdir_return {
    FILEMENU_RETURN = 0,
    FILEMENU_CLIPBOARD,
    FILEMENU_CURFOLDER
};

extern menu_entry fs_menu_startdir[];

extern gpt_entry_rule gpt_fs_rules[];

extern menu_entry mmcmenu_start[];