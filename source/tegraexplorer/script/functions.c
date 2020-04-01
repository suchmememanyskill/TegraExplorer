#include <string.h>
#include <stdlib.h>
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../emmc/emmc.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../storage/emummc.h"
#include "parser.h"
#include "../common/common.h"
#include "../fs/fsactions.h"
#include "variables.h"
#include "../utils/utils.h"
#include "functions.h"
#include "../fs/fsutils.h"
#include "../../utils/sprintf.h"
#include "../fs/fsactions.h"

extern FIL scriptin;
extern char **argv;
extern u32 argc;
extern int forceExit;

int parseIntInput(char *in, int *out){
    if (in[0] == '@'){
        if (str_int_find(argv[0], out))
            return -1;
    }
    else
        *out = atoi(in);
    
    return 0;
}

int parseJmpInput(char *in, u64 *out){
    if (in[0] == '?'){
        if (str_jmp_find(argv[0], out))
            return -1;
        else
            return 0;
    }
    else
        return -1;
}

int parseStringInput(char *in, char **out){
    if (in[0] == '$'){
        if (str_str_find(in, out))
            return -1;
        else
            return 0;
    }
    else{
        *out = in; 
        return 0;
    }
}

u32 currentcolor = COLOR_WHITE;
int part_printf(){
    char *toprint;
    if (parseStringInput(argv[0], &toprint))
        return -1;
        
    SWAPCOLOR(currentcolor);
    gfx_printf(toprint);
    gfx_printf("\n");
    return 0;
}

int part_print_int(){
    int toprint;
    if (parseIntInput(argv[0], &toprint))
        return -1;
    
    SWAPCOLOR(currentcolor);
    gfx_printf("%s: %d\n", argv[0], toprint);
    return 0;
}

int part_Wait(){
    int arg;
    u32 begintime;
    SWAPCOLOR(currentcolor);

    if (parseIntInput(argv[0], &arg))
        return -1;

    begintime = get_tmr_s();

    while (begintime + arg > get_tmr_s()){
        gfx_printf("\r<Wait %d seconds> ", (begintime + arg) - get_tmr_s());
    }

    gfx_printf("\r                 \r");
    return 0;
}

int part_if(){
    int condition;
    if (parseIntInput(argv[0], &condition))
        return -1;

    getfollowingchar('{');

    if (condition)
        return 0;
    else {
        skipbrackets();
        return 0;
    }
}

int part_Math(){
    int left, right;
    if (parseIntInput(argv[0], &left))
        return -1;
    if (parseIntInput(argv[2], &right))
        return -1;
    
    switch (argv[1][0]){
        case '+':
            return left + right;
        case '-':
            return left - right;
        case '*':
            return left * right;
        case '/':
            return left * right;
    }
    return -1;
}

int part_Check(){
    int left, right;
    if (parseIntInput(argv[0], &left))
        return -1;
    if (parseIntInput(argv[2], &right))
        return -1;

    if (!strcmp(argv[1], "=="))
        return (left == right);
    else if (!strcmp(argv[1], "!="))
        return (left != right);
    else if (!strcmp(argv[1], ">="))
        return (left >= right);
    else if (!strcmp(argv[1], "<="))
        return (left <= right);
    else if (!strcmp(argv[1], ">"))
        return (left > right);
    else if (!strcmp(argv[1], "<"))
        return (left < right);
    else
        return -1;
}

int part_SetInt(){
    int out;
    parseIntInput(argv[0], &out);
    return out;
}

int part_SetString(){
    char *arg0;
    if (parseStringInput(argv[0], &arg0))
        return -1;
    if (argv[1][0] != '$')
        return -1;

    str_str_add(argv[1], arg0);
    return 0;
}

int part_SetStringIndex(){
    int index;
    char *out;
    if (parseIntInput(argv[0], &index))
        return -1;
    if (argv[1][0] != '$')
        return -1;
    if (str_str_index(index, &out))
        return -1;
    str_str_add(argv[1], out);
    return 0;
}

int part_goto(){
    u64 target = 0;
    if (parseJmpInput(argv[0], &target))
        return -1;
    f_lseek(&scriptin, target);
    return 0;
}

int part_invert(){
    int arg;
    if (parseIntInput(argv[0], &arg))
        return -1;
    return (arg) ? 0 : 1;
}

int part_fs_exists(){
    char *path;
    if (parseStringInput(argv[0], &path))
        return -1;
    
    return fsutil_checkfile(path);
}

int part_ConnectMMC(){
    char *arg;
    parseStringInput(argv[0], &arg);

    if (!strcmp(arg, "SYSMMC"))
        connect_mmc(SYSMMC);
    else if (!strcmp(arg, "EMUMMC"))
        connect_mmc(EMUMMC);
    else
        return -1;

    return 0;
}

int part_MountMMC(){
    char *arg;
    parseStringInput(argv[0], &arg);
    return mount_mmc(arg, 2);
}

int part_Pause(){
    int res;

    while (btn_read() != 0);

    res = btn_wait();  

    str_int_add("@BTN_POWER", (res & BTN_POWER));
    str_int_add("@BTN_VOL+", (res & BTN_VOL_UP));
    str_int_add("@BTN_VOL-", (res & BTN_VOL_DOWN));
    
    return res;
}

int part_addstrings(){
    char *combined, *left, *middle;
    if (parseStringInput(argv[0], &left))
        return -1;
    if (parseStringInput(argv[1], &middle))
        return -1;
    if (argv[2][0] != '$')
        return -1;
    
    combined = calloc(strlen(left) + strlen(middle) + 1, sizeof(char));
    sprintf(combined, "%s%s", left, middle);
    
    str_str_add(argv[2], combined);
    free(combined);
    return 0;
}

int part_setColor(){
    char *arg;
    if (parseStringInput(argv[0], &arg))
        return -1;

    if (!strcmp(arg, "RED"))
        currentcolor = COLOR_RED;
    else if (!strcmp(arg, "ORANGE"))
        currentcolor = COLOR_ORANGE;
    else if (!strcmp(arg, "YELLOW"))
        currentcolor = COLOR_YELLOW;
    else if (!strcmp(arg, "GREEN"))
        currentcolor = COLOR_GREEN;
    else if (!strcmp(arg, "BLUE"))
        currentcolor = COLOR_BLUE;
    else if (!strcmp(arg, "VIOLET"))
        currentcolor = COLOR_VIOLET;
    else if (!strcmp(arg, "WHITE"))
        currentcolor = COLOR_WHITE;
    else
        return -1;

    return 0;
}

int part_Exit(){
    forceExit = true;
    return 0;
}

int part_fs_Move(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return -1;
    if (parseStringInput(argv[1], &right))
        return -1;

    int res;
    res = f_rename(left, right);
    if (res)
        res = f_rename(left, right);
    
    return res;
}

int part_fs_Delete(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return -1;

    int res;
    res = f_unlink(arg);
    if (res)
        res = f_unlink(arg);
    
    return res;
}

int part_fs_DeleteRecursive(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return -1;

    return fsact_del_recursive(arg);
}

int part_fs_Copy(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return -1;
    if (parseStringInput(argv[1], &right))
        return -1;

    return fsact_copy(left, right, COPY_MODE_PRINT);
}

int part_fs_CopyRecursive(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return -1;
    if (parseStringInput(argv[1], &right))
        return -1;

    return fsact_copy_recursive(left, right);
}

int part_fs_MakeDir(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return -1;

    int res;
    res = f_mkdir(arg);
    if (res)
        res = f_mkdir(arg);
    
    return res;
}

DIR dir;
FILINFO fno;
int isdirvalid = false;
int part_fs_OpenDir(){
    char *path;

    if (parseStringInput(argv[0], &path))
        return -1;

    if (f_opendir(&dir, path))
        return -1;
    
    isdirvalid = true;
    str_int_add("@ISDIRVALID", isdirvalid);
    return 0;
}

int part_fs_CloseDir(){
    if (!isdirvalid)
        return 0;

    f_closedir(&dir);
    isdirvalid = false;
    str_int_add("@ISDIRVALID", isdirvalid);
    return 0;
}

int part_fs_ReadDir(){
    if (!isdirvalid)
        return -1;

    if (!f_readdir(&dir, &fno) && fno.fname[0]){
        str_str_add("$FSOBJNAME", fno.fname);
        str_int_add("@ISDIR", (fno.fattrib & AM_DIR) ? 1 : 0);
    }
    else {
        part_fs_CloseDir();
    }

    return 0;
}

int part_setPrintPos(){
    int left, right;

    if (parseIntInput(argv[0], &left))
        return -1;

    if (parseIntInput(argv[1], &right))
        return -1;

    if (left > 42)
        return -1;

    if (right > 78)
        return -1;

    gfx_con_setpos(left * 16, right * 16);
    return 0;
}

str_fnc_struct functions[] = {
    {"printf", part_printf, 1},
    {"printInt", part_print_int, 1},
    {"setPrintPos", part_setPrintPos, 2},
    {"if", part_if, 1},
    {"math", part_Math, 3},
    {"check", part_Check, 3},
    {"setInt", part_SetInt, 1},
    {"goto", part_goto, 1},
    {"setString", part_SetString, 2},
    {"setStringIndex", part_SetStringIndex, 2},
    {"setColor", part_setColor, 1},
    {"combineStrings", part_addstrings, 3},
    {"invert", part_invert, 1},
    {"fs_exists", part_fs_exists, 1},
    {"fs_move", part_fs_Move, 2},
    {"fs_mkdir", part_fs_MakeDir, 1},
    {"fs_del", part_fs_Delete, 1},
    {"fs_delRecursive", part_fs_DeleteRecursive, 1},
    {"fs_copy", part_fs_Copy, 2},
    {"fs_copyRecursive", part_fs_CopyRecursive, 2},
    {"fs_openDir", part_fs_OpenDir, 1},
    {"fs_closeDir", part_fs_CloseDir, 0},
    {"fs_readDir", part_fs_ReadDir, 0},
    {"mmc_connect", part_ConnectMMC, 1},
    {"mmc_mount", part_MountMMC, 1},
    {"pause", part_Pause, 0},
    {"wait", part_Wait, 1},
    {"exit", part_Exit, 0},
    {NULL, NULL, 0}
};

int run_function(char *func_name, int *out){
    for (u32 i = 0; functions[i].key != NULL; i++){
        if (!strcmp(functions[i].key, func_name)){
            if (argc != functions[i].arg_count)
                return -2;

            *out = functions[i].value();
            if (*out < 0)
                return -1;
            return 0;
        }
    }
    return -1;
}