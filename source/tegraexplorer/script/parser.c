#include <string.h>
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
#include "functions.h"
#include "variables.h"


u32 countchars(char* in, char target) {
    u32 len = strlen(in);
    u32 count = 0;
    for (u32 i = 0; i < len; i++) {
        if (in[i] == target)
            count++;
    }
    return count;
}

char **argv = NULL;
u32 argc;
u32 splitargs(char* in) {
    // arg like '5, "6", @arg7'
    u32 i, current = 0, count = countchars(in, ',') + 1, len = strlen(in), curcount = 0;

    if (argv != NULL) {
        for (i = 0; argv[i] != NULL; i++)
            free(argv[i]);
        free(argv);
    }
        
    argv = calloc(count + 1, sizeof(char*));

    for (i = 0; i < count; i++)
        argv[i] = calloc(128, sizeof(char));
    

    for (i = 0; i < len && curcount < count; i++) {
        if (in[i] == ',') {
            curcount++;
            current = 0;
        }
        else if (in[i] == '@' || in[i] == '$') {
            while (in[i] != ',' && in[i] != ' ' && in[i] != ')' && i < len) {
                argv[curcount][current++] = in[i++];
            }
            i--;
        }
        else if (in[i] >= '0' && in[i] <= '9')
            argv[curcount][current++] = in[i];
        else if (in[i] == '"') {
            i++;
            while (in[i] != '"') {
                argv[curcount][current++] = in[i++];
            }
        }
    }
    return count;
}

FIL scriptin;
UINT endByte = 0;
int forceExit = false;
char currentchar = 0;

char getnextchar(){
    f_read(&scriptin, &currentchar, sizeof(char), &endByte);
    
    if (sizeof(char) != endByte)
        forceExit = true;

    //gfx_printf("|%c|", currentchar);
    return currentchar;
}

void getfollowingchar(char end){
    while (currentchar != end){
        if (currentchar == '"'){
            while (getnextchar() != '"');
        }
        getnextchar();
    }
}

void getnextvalidchar(){
    while ((!((currentchar >= 'A' && currentchar <= 'Z') || (currentchar >= 'a' && currentchar <= 'z') || currentchar == '#' || currentchar == '@') && !f_eof(&scriptin)) /*|| currentchar == ';' */)
        getnextchar();
}

char *makestr(u32 size, char ignore){
    char *str;
    u32 count = 0;

    str = calloc(size + 1, sizeof(char));
    for (u32 i = 0; i < size; i++){
        getnextchar();
        if (ignore != 0 && ignore == currentchar)
            continue;
                
        str[count++] = currentchar;
    }
        
    return str;
}
/*
char *makestrstrip(u32 size){
    char *str;
    int count = 0;

    str = calloc(size + 1, sizeof(char));
    for (u32 i = 0; i < size;){
        if (currentchar == '"'){
            getnextchar();
            i++;
            while (getnextchar() != '"')
                i++;
        }
        else if (currentchar == ' '){
            while (getnextchar() == ' ')
                i++;
        }
        else{
            getnextchar();
            i++;
        }
            
        str[count++] = currentchar;
    }
        
    return str;
}
*/

char *readtilchar(char end, char ignore){ // this will strip spaces unless it's enclosed in a ""
    FSIZE_t offset, size;

    offset = f_tell(&scriptin);
    getfollowingchar(end);
    size = f_tell(&scriptin) - offset;
    f_lseek(&scriptin, offset - 1);

    return makestr((u32)size, ignore);
}


char *funcbuff = NULL;
void functionparser(){
    char *unsplitargs;
    FSIZE_t fileoffset;
    u32 argsize = 0;

    if (funcbuff != NULL)
        free(funcbuff);

    //gfx_printf("getting func %c\n", currentchar);
    funcbuff = readtilchar('(', ' ');
    /*calloc(20, sizeof(char));
    for (int i = 0; i < 19 && currentchar != '(' && currentchar != ' '; i++){
        funcbuff[i] = currentchar;
        getnextchar();
    }
    */

    getfollowingchar('(');
    getnextchar();

    //gfx_printf("getting arg len %c\n", currentchar);

    /*
    fileoffset = f_tell(&scriptin);
    getfollowingchar(')');
    argsize = f_tell(&scriptin) - fileoffset;
    f_lseek(&scriptin, fileoffset - 1);

    getnextchar();

    gfx_printf("getting args %c\n", currentchar);

    unsplitargs = calloc(argsize + 1, sizeof(char));
    for (int i = 0; i < argsize; i++){
        unsplitargs[i] = currentchar;
        getnextchar();
    }
    */
    unsplitargs = readtilchar(')', 0);

    //gfx_printf("parsing args %c\n", currentchar);
    argc = splitargs(unsplitargs);
    getnextchar();
    getnextchar();

    /*
    gfx_printf("\n\nFunc: %s\n", funcbuff, currentchar);
    gfx_printf("Split: %s\n", unsplitargs);

    for (int i = 0; i < argc; i++)
        gfx_printf("%d | %s\n", i, argv[i]);
    */

    //gfx_printf("\ncurrent char: %c", currentchar);
    free(unsplitargs);
}

char *gettargetvar(){
    char *variable = NULL;
    FSIZE_t fileoffset;
    u32 varsize = 0;

    /*
    fileoffset = f_tell(&scriptin);
    getfollowingchar('=');
    varsize = f_tell(&scriptin) - fileoffset;
    f_lseek(&scriptin, fileoffset - 1);

    variable = calloc(varsize + 1, sizeof(char));

    getnextchar();
    for (int i = 0; i < varsize; i++){
        if (currentchar == ' ')
            break;

        variable[i] = currentchar;
        getnextchar();
    }
    */

    variable = readtilchar('=', ' ');

    getfollowingchar('=');
    getnextchar();

    return variable;
}

void mainparser(){
    char *variable = NULL;
    int res, out = 0;
    FSIZE_t fileoffset;
    u32 varsize = 0;

    getnextvalidchar();

    if (f_eof(&scriptin))
        return;

    if (currentchar == '#'){
        getfollowingchar('\n');
        return;
    }

    if (currentchar == '@'){
        variable = gettargetvar();
        getnextvalidchar();
    }

    functionparser();
    /*
    if (variable != NULL)
        gfx_printf("target: %s\n", variable);
    */

    //gfx_printf("Func: %s\n", funcbuff);
    res = run_function(funcbuff, &out);
    str_int_add("@RESULT", res);

    if (variable != NULL)
        str_int_add(variable, res);

    //gfx_printf("\nGoing to next func %c\n", currentchar);
    free(variable);
}

void skipbrackets(){
    u32 bracketcounter = 0;

    getfollowingchar('{');
    getnextchar();

    while (currentchar != '}' || bracketcounter != 0){
        if (currentchar == '{')
            bracketcounter++;
        else if (currentchar == '}')
            bracketcounter--;

        getnextchar();
    }
}

void tester(char *path){
    int res;

    gfx_clearscreen();

    res = f_open(&scriptin, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        gfx_errDisplay("ParseScript", res, 1);
        return;
    }
    
    //add builtin vars
    str_int_add("@EMUMMC", emu_cfg.enabled);
    str_int_add("@RESULT", 0);
    str_int_add("@BTN_POWER", 0);
    str_int_add("@BTN_VOL+", 0);
    str_int_add("@BTN_VOL-", 0);

    printerrors = false;

    while (!f_eof(&scriptin)){
        mainparser();
    }

    printerrors = true;
    //str_int_printall();

    f_close(&scriptin);
    str_int_clear();
    btn_wait();
}