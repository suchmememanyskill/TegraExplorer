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

extern FIL scriptin;
extern char **argv;
extern u32 argc;

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

int part_printf(){
    gfx_printf(argv[0]);
    gfx_printf("\n");
    return 0;
}

int part_print_int(){
    int toprint;
    if (parseIntInput(argv[0], &toprint))
        return -1;
    gfx_printf("%s: %d\n", argv[0], toprint);
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
    return 0;
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

int part_goto(){
    u64 target = 0;
    if (parseJmpInput(argv[0], &target))
        return -1;
    f_lseek(&scriptin, target);
    return 0;
}

str_fnc_struct functions[] = {
    {"printf", part_printf, 1},
    {"printInt", part_print_int, 1},
    {"if", part_if, 1},
    {"math", part_Math, 3},
    {"check", part_Check, 3},
    {"setInt", part_SetInt, 1},
    {"goto", part_goto, 1},
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