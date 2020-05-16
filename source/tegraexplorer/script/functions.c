#include <string.h>
#include <stdlib.h>
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../emmc/emmc.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../hid/hid.h"
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
#include "../emmc/emmcoperations.h"
#include "../emmc/emmcmenu.h"

extern FIL scriptin;
extern char **argv;
extern u32 argc;
extern int forceExit;
extern short currentlyMounted;

int parseIntInput(char *in, int *out){
    if (in[0] == '@'){
        if (str_int_find(in, out))
            return -1;
    }
    else
        *out = atoi(in);
    
    return 0;
}
/*
int parseJmpInput(char *in, u64 *out){
    if (in[0] == '?'){
        if (str_jmp_find(in, out))
            return -1;
        else
            return 0;
    }
    else
        return -1;
}
*/

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

#pragma GCC push_options
#pragma GCC optimize ("Os")

u32 currentcolor = COLOR_WHITE;
int part_printf(){
    SWAPCOLOR(currentcolor);
    for (int i = 0; i < argc; i++){
        if (argv[i][0] == '@'){
            int toprintint;
            if (parseIntInput(argv[i], &toprintint))
                return INFUNC_FAIL;

            gfx_printf("%d", toprintint);
        }
        else {
            char *toprintstring;
            if (parseStringInput(argv[i], &toprintstring))
                return INFUNC_FAIL;

            gfx_printf(toprintstring);
        }
    }

    gfx_printf("\n");
    return 0;
}

int part_print_int(){
    int toprint;
    if (parseIntInput(argv[0], &toprint))
        return INFUNC_FAIL;
    
    SWAPCOLOR(currentcolor);
    gfx_printf("%s: %d\n", argv[0], toprint);
    return 0;
}

int part_Wait(){
    int arg;
    u32 begintime;
    SWAPCOLOR(currentcolor);

    if (parseIntInput(argv[0], &arg))
        return INFUNC_FAIL;

    begintime = get_tmr_s();

    while (begintime + arg > get_tmr_s()){
        gfx_printf("\r<Wait %d seconds> ", (begintime + arg) - get_tmr_s());
    }

    gfx_printf("\r                 \r");
    return 0;
}

int part_Check(){
    int left, right;
    if (parseIntInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseIntInput(argv[2], &right))
        return INFUNC_FAIL;

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
        return INFUNC_FAIL;
}

int part_if(){
    int condition;
    if (parseIntInput(argv[0], &condition))
        return INFUNC_FAIL;

    getfollowingchar('{');

    if (!condition)
        skipbrackets();
    
    return 0;

    /*
    if (condition)
        return 0;
    else {
        skipbrackets();
        return 0;
    }
    */
}

int part_if_args(){
    int condition;
    if ((condition = part_Check()) < 0)
        return INFUNC_FAIL;

    getfollowingchar('{');

    if (!condition)
        skipbrackets();
    
    return 0;
}

int part_Math(){
    int left, right;
    if (parseIntInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseIntInput(argv[2], &right))
        return INFUNC_FAIL;
    
    switch (argv[1][0]){
        case '+':
            return left + right;
        case '-':
            return left - right;
        case '*':
            return left * right;
        case '/':
            return left / right;
    }
    return INFUNC_FAIL;
}

int part_SetInt(){
    int out;
    parseIntInput(argv[0], &out);
    return out;
}

int part_SetString(){
    char *arg0;
    if (parseStringInput(argv[0], &arg0))
        return INFUNC_FAIL;
    if (argv[1][0] != '$')
        return INFUNC_FAIL;

    str_str_add(argv[1], arg0);
    return 0;
}

int part_SetStringIndex(){
    int index;
    char *out;
    if (parseIntInput(argv[0], &index))
        return INFUNC_FAIL;
    if (argv[1][0] != '$')
        return INFUNC_FAIL;
    if (str_str_index(index, &out))
        return INFUNC_FAIL;

    str_str_add(argv[1], out);
    return 0;
}

int part_goto(){
    int target = 0;
    if (parseIntInput(argv[0], &target))
        return INFUNC_FAIL;

    str_int_add("@RETURN", (int)f_tell(&scriptin));

    f_lseek(&scriptin, target);
    return 0;
}

int part_invert(){
    int arg;
    if (parseIntInput(argv[0], &arg))
        return INFUNC_FAIL;
    return (arg) ? 0 : 1;
}

int part_fs_exists(){
    char *path;
    if (parseStringInput(argv[0], &path))
        return INFUNC_FAIL;
    
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
        return INFUNC_FAIL;

    return 0;
}

int part_MountMMC(){
    char *arg;
    parseStringInput(argv[0], &arg);
    return mount_mmc(arg, 2);
}

int part_Pause(){
    Inputs *input = hidWaitMask(KEY_A | KEY_B | KEY_X | KEY_Y | KEY_POW | KEY_VOLP | KEY_VOLM | KEY_LUP | KEY_LDOWN | KEY_LLEFT | KEY_LRIGHT);

    str_int_add("@BTN_POWER", input->pow);
    str_int_add("@BTN_VOL+", input->volp);
    str_int_add("@BTN_VOL-", input->volm);
    str_int_add("@BTN_A", input->a);
    str_int_add("@BTN_B", input->b);
    str_int_add("@BTN_X", input->x);
    str_int_add("@BTN_Y", input->y);
    str_int_add("@BTN_UP", input->Lup);
    str_int_add("@BTN_DOWN", input->Ldown);
    str_int_add("@BTN_LEFT", input->Lleft);
    str_int_add("@BTN_RIGHT", input->Lright);
    str_int_add("@JOYCONN", hidConnected());
    
    return input->buttons;
}

int part_addstrings(){
    char *combined, *left, *middle;
    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &middle))
        return INFUNC_FAIL;
    if (argv[2][0] != '$')
        return INFUNC_FAIL;
    
    combined = calloc(strlen(left) + strlen(middle) + 1, sizeof(char));
    sprintf(combined, "%s%s", left, middle);
    
    str_str_add(argv[2], combined);
    free(combined);
    return 0;
}

int part_setColor(){
    char *arg;
    if (parseStringInput(argv[0], &arg))
        return INFUNC_FAIL;

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
        return INFUNC_FAIL;

    return 0;
}

int part_Exit(){
    forceExit = true;
    return 0;
}

int part_fs_Move(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &right))
        return INFUNC_FAIL;

    int res;
    res = f_rename(left, right);
    if (res)
        res = f_rename(left, right);
    
    return res;
}

int part_fs_Delete(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return INFUNC_FAIL;

    int res;
    res = f_unlink(arg);
    if (res)
        res = f_unlink(arg);
    
    return res;
}

int part_fs_DeleteRecursive(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return INFUNC_FAIL;

    return fsact_del_recursive(arg);
}

int part_fs_Copy(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &right))
        return INFUNC_FAIL;

    return fsact_copy(left, right, COPY_MODE_PRINT);
}

int part_fs_CopyRecursive(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &right))
        return INFUNC_FAIL;

    return fsact_copy_recursive(left, right);
}

int part_fs_MakeDir(){
    char *arg;

    if (parseStringInput(argv[0], &arg))
        return INFUNC_FAIL;

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
        return INFUNC_FAIL;

    if (f_opendir(&dir, path))
        return INFUNC_FAIL;
    
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
        return INFUNC_FAIL;

    if (!f_readdir(&dir, &fno) && fno.fname[0]){
        str_str_add("$FILENAME", fno.fname);
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
        return INFUNC_FAIL;

    if (parseIntInput(argv[1], &right))
        return INFUNC_FAIL;

    if (left > 78)
        return INFUNC_FAIL;

    if (right > 42)
        return INFUNC_FAIL;

    gfx_con_setpos(left * 16, right * 16);
    return 0;
}

int part_stringcompare(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &right))
        return INFUNC_FAIL;

    return (strcmp(left, right)) ? 0 : 1;
}

int part_fs_combinePath(){
    char *combined, *left, *middle;
    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &middle))
        return INFUNC_FAIL;
    if (argv[2][0] != '$')
        return INFUNC_FAIL;
    
    combined = fsutil_getnextloc(left, middle);
    
    str_str_add(argv[2], combined);
    free(combined);
    return 0;
}

int part_mmc_dumpPart(){
    char *left, *right;

    if (parseStringInput(argv[0], &left))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &right))
        return INFUNC_FAIL;

    if (!strcmp(left, "BOOT")){
        return emmcDumpBoot(right);
    }
    else {
        return emmcDumpSpecific(left, right);
    }
}

int part_mmc_restorePart(){
    char *path;

    if (parseStringInput(argv[0], &path))
        return INFUNC_FAIL;

    if (currentlyMounted < 0)
        return INFUNC_FAIL;

    return mmcFlashFile(path, currentlyMounted, false);   
}

int part_fs_extractBisFile(){
    char *path, *outfolder;

    if (parseStringInput(argv[0], &path))
        return INFUNC_FAIL;
    if (parseStringInput(argv[1], &outfolder))
        return INFUNC_FAIL;

    return extract_bis_file(path, outfolder);
}

int part_clearscreen(){
    gfx_clearscreen();
    return 0;
}

int part_getPos(){
    return (int)f_tell(&scriptin);
}

int part_subString(){
    char *str, *sub;
    int start, size;

    if (parseStringInput(argv[0], &str))
        return INFUNC_FAIL;
    if (parseIntInput(argv[1], &start))
        return INFUNC_FAIL;
    if (parseIntInput(argv[2], &size))
        return INFUNC_FAIL;
    if (argv[3][0] != '$')
        return INFUNC_FAIL;

    if (start >= strlen(str))
        return INFUNC_FAIL;

    sub = utils_copyStringSize(str + start, size);
    str_str_add(argv[3], sub);
    free(sub);
    return 0;
}

int part_inputString(){
    char *start, *out;
    int len;

    if (parseStringInput(argv[0], &start))
        return INFUNC_FAIL;
    if (parseIntInput(argv[1], &len))
        return INFUNC_FAIL;
    if (argv[2][0] != '$')
        return INFUNC_FAIL;

    if (len > 39)
        return INFUNC_FAIL;

    out = utils_InputText(start, len);
    if (out == NULL)
        return 1;

    str_str_add(argv[2], out);
    free(out);
    return 0;
}

int part_strLen(){
    char *in;

    if (parseStringInput(argv[0], &in))
        return INFUNC_FAIL;

    return strlen(in);
}

str_fnc_struct functions[] = {
    {"printf", part_printf, 255},
    {"printInt", part_print_int, 1},
    {"setPrintPos", part_setPrintPos, 2},
    {"clearscreen", part_clearscreen, 0},
    {"if", part_if, 1},
    {"if", part_if_args, 3}, // function overloading
    {"math", part_Math, 3},
    {"check", part_Check, 3},
    {"setInt", part_SetInt, 1},
    {"goto", part_goto, 1},
    {"setString", part_SetString, 2},
    {"setStringIndex", part_SetStringIndex, 2},
    {"setColor", part_setColor, 1},
    {"combineStrings", part_addstrings, 3},
    {"compareStrings", part_stringcompare, 2},
    {"subString", part_subString, 4},
    {"inputString", part_inputString, 3},
    {"stringLength", part_strLen, 1},
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
    {"fs_combinePath", part_fs_combinePath, 3},
    {"fs_extractBisFile", part_fs_extractBisFile, 2},
    {"mmc_connect", part_ConnectMMC, 1},
    {"mmc_mount", part_MountMMC, 1},
    {"mmc_dumpPart", part_mmc_dumpPart, 2},
    {"mmc_restorePart", part_mmc_restorePart, 1},
    {"getPosition", part_getPos, 0},
    {"pause", part_Pause, 0},
    {"wait", part_Wait, 1},
    {"exit", part_Exit, 0},
    {NULL, NULL, 0}
};

#pragma GCC pop_options

int run_function(char *func_name, int *out){
    for (u32 i = 0; functions[i].key != NULL; i++){
        if (!strcmp(functions[i].key, func_name)){
            if (argc != functions[i].arg_count && functions[i].arg_count != 255)
                continue;

            *out = functions[i].value();
            return (*out == INFUNC_FAIL) ? -1 : 0;
        }
    }
    return -2;
}