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
#include "../fs/fsreader.h"
#include "../utils/utils.h"

u32 countchars(char* in, char target) {
    u32 len = strlen(in);
    u32 count = 0;
    for (u32 i = 0; i < len; i++) {
        if (in[i] == '"'){
            i++;
            while (in[i] != '"'){
                i++;
                if (i >= len)
                    return -1;
            }
        }
        if (in[i] == target)
            count++;
    }
    return count;
}

char **argv = NULL;
u32 argc;
u32 splitargs(char* in) {
    // arg like '5, "6", @arg7'
    u32 i, current = 0, count = 1, len = strlen(in), curcount = 0;

    if ((count += countchars(in, ',')) < 0){
        return 0;
    }

    /*
    if (argv != NULL) {
        for (i = 0; argv[i] != NULL; i++)
            free(argv[i]);
        free(argv);
    }
    */
        
    argv = calloc(count + 1, sizeof(char*));

    for (i = 0; i < count; i++)
        argv[i] = calloc(96, sizeof(char));
    

    for (i = 0; i < len && curcount < count; i++) {
        if (in[i] == ',') {
            curcount++;
            current = 0;
        }
        else if (in[i] == '@' || in[i] == '$' || in[i] == '?') {
            while (in[i] != ',' && in[i] != ' ' && in[i] != ')' && i < len) {
                argv[curcount][current++] = in[i++];
            }
            i--;
        }
        else if ((in[i] >= '0' && in[i] <= '9') || (in[i] >= '<' && in[i] <= '>') || in[i] == '+' || in[i] == '-' || in[i] == '*' || in[i] == '/')
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
    while (currentchar != end && !f_eof(&scriptin)){
        if (currentchar == '"'){
            while (getnextchar() != '"' && !f_eof(&scriptin));
        }
        getnextchar();
    }
}

void getnextvalidchar(){
    while ((!((currentchar >= '?' && currentchar <= 'Z') || (currentchar >= 'a' && currentchar <= 'z') || currentchar == '#') && !f_eof(&scriptin)) /*|| currentchar == ';' */)
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

char *readtilchar(char end, char ignore){
    FSIZE_t offset, size;

    offset = f_tell(&scriptin);
    getfollowingchar(end);
    size = f_tell(&scriptin) - offset;

    if (size <= 0)
        return NULL;

    f_lseek(&scriptin, offset - 1);

    return makestr((u32)size, ignore);
}


char *funcbuff = NULL;
void functionparser(){
    char *unsplitargs;

    /*
    if (funcbuff != NULL)
        free(funcbuff);
    */

    funcbuff = readtilchar('(', ' ');

    getfollowingchar('(');
    getnextchar();

    unsplitargs = readtilchar(')', 0);

    if (unsplitargs != NULL){
        argc = splitargs(unsplitargs);
        getnextchar();
    }
    else {
        argc = 0;
    }
    getnextchar();

    free(unsplitargs);
}

char *gettargetvar(){
    char *variable = NULL;

    variable = readtilchar('=', ' ');

    getfollowingchar('=');
    getnextchar();

    return variable;
}

void mainparser(){
    char *variable = NULL;
    int res, out = 0;

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

    /*
    if (currentchar == '?'){
        char *jumpname;
        jumpname = readtilchar(';', ' ');
        getnextchar();
        str_jmp_add(jumpname, f_tell(&scriptin));
        getfollowingchar('\n');
        return;
    }
    */

    functionparser();

    res = run_function(funcbuff, &out);
    if (res < 0){
        printerrors = true;
        //gfx_printf("%s|%s|%d", funcbuff, argv[0], argc);
        //btn_wait();
        gfx_errDisplay("mainparser", ERR_PARSE_FAIL, f_tell(&scriptin));
        forceExit = true;
        //gfx_printf("Func: %s\nArg1: %s\n", funcbuff, argv[0]);
    }
    else {
        str_int_add("@RESULT", out);

        if (variable != NULL)
            str_int_add(variable, out);
    }

    //gfx_printf("\nGoing to next func %c\n", currentchar);

    if (funcbuff != NULL){
         free(funcbuff);
         funcbuff = NULL;
    }

    if (argv != NULL) {
        for (int i = 0; argv[i] != NULL; i++)
            free(argv[i]);
        free(argv);
        argv = NULL;
    }

    if (variable != NULL){
        free(variable);
    }
}

void skipbrackets(){
    u32 bracketcounter = 0;

    getfollowingchar('{');
    getnextchar();

    while ((currentchar != '}' || bracketcounter != 0) && !f_eof(&scriptin)){
        if (currentchar == '{')
            bracketcounter++;
        else if (currentchar == '}')
            bracketcounter--;

        getnextchar();
    }
}

extern u32 currentcolor;
extern char *currentpath;
void runScript(char *path){
    int res;
    char *path_local = NULL;
    forceExit = false;
    currentchar = 0;
    currentcolor = COLOR_WHITE;
    gfx_clearscreen();
    utils_copystring(path, &path_local);

    res = f_open(&scriptin, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        gfx_errDisplay("ParseScript", res, 1);
        return;
    }

    printerrors = false;
    
    //add builtin vars
    str_int_add("@EMUMMC", emu_cfg.enabled);
    str_int_add("@RESULT", 0);
    str_str_add("$CURRENTPATH", currentpath);

    //str_int_printall();

    while (!f_eof(&scriptin) && !forceExit){
        mainparser();
    }

    printerrors = true;
    //str_int_printall();

    f_close(&scriptin);
    str_int_clear();
    //str_jmp_clear();
    str_str_clear();
    free(path_local);
    //btn_wait();
}