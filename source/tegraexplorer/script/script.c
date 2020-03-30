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
#include "script.h"
#include "../common/common.h"
#include "../fs/fsactions.h"


u32 countchars(char* in, char target) {
    u32 len = strlen(in);
    u32 count = 0;
    for (u32 i = 0; i < len; i++) {
        if (in[i] == target)
            count++;
    }
    return count;
}

char **args = NULL;
u32 splitargs(char* in) {
    // arg like '5, "6", @arg7'
    u32 i, current = 0, count = countchars(in, ',') + 1, len = strlen(in), curcount = 0;

    if (args != NULL) {
        for (i = 0; args[i] != NULL; i++)
            free(args[i]);
        free(args);
    }
        
    args = calloc(count + 1, sizeof(char*));

    for (i = 0; i < count; i++)
        args[i] = calloc(128, sizeof(char));
    

    for (i = 0; i < len && curcount < count; i++) {
        if (in[i] == ',') {
            curcount++;
            current = 0;
        }
        else if (in[i] == '@' || in[i] == '$') {
            while (in[i] != ',' && in[i] != ' ' && in[i] != ')' && i < len) {
                args[curcount][current++] = in[i++];
            }
            i--;
        }
        else if (in[i] >= '0' && in[i] <= '9')
            args[curcount][current++] = in[i];
        else if (in[i] == '"') {
            i++;
            while (in[i] != '"') {
                args[curcount][current++] = in[i++];
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

    gfx_printf("|%c|", currentchar);
    return currentchar;
}

void getfollowingchar(char end){
    while (currentchar != end){
        getnextchar();
        if (currentchar == '"'){
            while (getnextchar() != '"');
        }
    }
}

void getnextvalidchar(){
    while (!((currentchar >= '0' && currentchar <= 'Z') || (currentchar >= 'a' && currentchar <= 'z') || currentchar == '#' || currentchar == '@'))
        getnextchar();
}

void mainparser(){
    char *unsplitargs;
    FSIZE_t curoffset;
    u32 argsize = 0;

    getnextvalidchar();

    if (currentchar == '#'){
        getfollowingchar('\n');
        return;
    }

    gfx_printf("Getting func name...\n");
    char *funcbuff = calloc(20, sizeof(char));
    for (int i = 0; i < 19 && currentchar != '(' && currentchar != ' '; i++){
        funcbuff[i] = currentchar;
        getnextchar();
    }
    gfx_printf("Skipping to next (...\n");
    getfollowingchar('(');
    getnextchar();

    curoffset = f_tell(&scriptin);
    gfx_printf("Skipping to next )...\n");
    getfollowingchar(')');
    argsize = f_tell(&scriptin) - curoffset;
    f_lseek(&scriptin, curoffset - 1);

    gfx_printf("Getting args...\nSize: %d\n",  argsize);
    getnextchar();
    unsplitargs = calloc(argsize, sizeof(char));
    for (int i = 0; i < argsize; i++){
        unsplitargs[i] = currentchar;
        getnextchar();
    }

    gfx_printf("Splitting args...\n");
    int argcount = splitargs(unsplitargs);

    gfx_printf("\n\nFunc: %s\n", funcbuff);
    gfx_printf("Split: %s\n", unsplitargs);

    for (int i = 0; i < argcount; i++)
        gfx_printf("%d | %s\n", i, args[i]);

    free(unsplitargs);
    free(funcbuff); 
}

void tester(char *path){
    int res;

    gfx_clearscreen();

    res = f_open(&scriptin, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        gfx_errDisplay("ParseScript", res, 1);
        return;
    }

    printerrors = false;


    mainparser();
    btn_wait();
}