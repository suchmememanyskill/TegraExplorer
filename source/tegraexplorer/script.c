#include <string.h>
#include "../mem/heap.h"
#include "gfx.h"
#include "fs.h"
#include "io.h"
#include "emmc.h"
#include "../utils/types.h"
#include "../libs/fatfs/ff.h"
#include "../utils/sprintf.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"
#include "../utils/util.h"
#include "../storage/emummc.h"
#include "script.h"

char func[11] = "", args[2][128] = {"", ""};
int res;

int Part_Delete(){
    return f_unlink(args[0]);
}

int Part_DeleteRecursive(){
    return del_recursive(args[0]);
}

int Part_Copy(){
    return copy(args[0], args[1], true, false);
}

int Part_RecursiveCopy(){
    return copy_recursive(args[0], args[1]);
}

int Part_MakeFolder(){
    return f_mkdir(args[0]);
}

int Part_ConnectMMC(){
    if (strcmpcheck(args[0], "SYSMMC"))
        connect_mmc(SYSMMC);
    if (strcmpcheck(args[0], "EMUMMC"))
        connect_mmc(EMUMMC);

    return 0;
}

int Part_MountMMC(){
    return mount_mmc(args[0], 2);
}

int Part_Print(){
    RESETCOLOR;
    gfx_printf(args[0]);
    gfx_printf("\n");
    return 0;
}

int Part_ErrorPrint(){
    gfx_printf("Errorcode: %d\n", res);
    return 0;
}

bool forceExit = false;
int Part_Exit(){
    forceExit = true;
    return 0;
}

u8 buttons_pressed = 0;
int Part_WaitOnUser(){
    buttons_pressed = btn_wait();
    return (buttons_pressed & BTN_POWER);
}

script_parts parts[] = {
    {"COPY", Part_Copy, 2},
    {"COPY-R", Part_RecursiveCopy, 2},
    {"MKDIR", Part_MakeFolder, 1},
    {"CON_MMC", Part_ConnectMMC, 1},
    {"MNT_MMC", Part_MountMMC, 1},
    {"PRINT", Part_Print, 1},
    {"ERRPRINT", Part_ErrorPrint, 0},
    {"EXIT", Part_Exit, 0},
    {"PAUSE", Part_WaitOnUser, 0},
    {"DEL", Part_Delete, 1},
    {"DEL-R", Part_DeleteRecursive, 1},
    {"NULL", NULL, -1}
};

int ParsePart(){
    int i;
    for (i = 0; parts[i].arg_amount != -1; i++){
        if (strcmpcheck(func, parts[i].name))
            return i;
    }
    forceExit = true;
    return -1;
}

FIL in;
UINT endByte = 0;

char GetNextByte(){
    char single;
    f_read(&in, &single, sizeof(char), &endByte);
    
    if (sizeof(char) != endByte)
        forceExit = true;

    return single;
}

void ParseScript(char* path){
    char currentchar;
    int strlength;
    bool inifstatement = false;
    forceExit = false;
    
    clearscreen();

    res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        message(COLOR_RED, "File Opening Failed\nErrcode %d", res);
        return;
    }

    while (!forceExit){
        currentchar = GetNextByte();

        if (endByte == 0 || currentchar == (char)EOF)
            break;

        switch(currentchar){
            case '{':
                if (!inifstatement)
                    while (currentchar != '}')
                        currentchar = GetNextByte();
                
                break;
            case '}':
                if (inifstatement)
                    inifstatement = false;

                break;
            case '<':
                strlength = 0;
                currentchar = GetNextByte();

                while (currentchar != '>'){
                    func[strlength++] = currentchar;
                    currentchar = GetNextByte();
                }
                func[strlength] = '\0';

                res = ParsePart();
                if (res == -1)
                    break;
                
                for (int i = 0; i < parts[res].arg_amount; i++){
                    while (currentchar != 0x22)
                        currentchar = GetNextByte();
                    
                    strlength = 0;
                    currentchar = GetNextByte();
                    while (currentchar != 0x22){
                        args[i][strlength++] = currentchar;
                        currentchar = GetNextByte();
                    }
                    args[i][strlength] = '\0';  

                    if (i < parts[res].arg_amount)
                        currentchar = GetNextByte();              
                }
                res = parts[res].handler();
                break;
            case '$':
                strlength = 0;
                currentchar = GetNextByte();
                
                while (currentchar != '$'){
                    func[strlength++] = currentchar;
                    currentchar = GetNextByte();
                }
                func[strlength] = '\0';

                if (strcmpcheck(func, "ERROR") || strcmpcheck(func, "TRUE")){
                    inifstatement = (res);
                }
                else if (strcmpcheck(func, "NOERROR") || strcmpcheck(func, "FALSE")){
                    inifstatement = (!res);
                }
                else if (strcmpcheck(func, "BTN_POWER")){
                    inifstatement = (buttons_pressed & BTN_POWER);
                }
                else if (strcmpcheck(func, "BTN_VOL+")){
                    inifstatement = (buttons_pressed & BTN_VOL_UP);
                }
                else if (strcmpcheck(func, "BTN_VOL-")){
                    inifstatement = (buttons_pressed & BTN_VOL_DOWN);
                }
                else if (strcmpcheck(func, "EMUMMC")){
                    inifstatement = (emu_cfg.enabled);
                }
                else if (strcmpcheck(func, "NOEMUMMC")){
                    inifstatement = (!emu_cfg.enabled);
                }

                if (inifstatement)
                    while(currentchar != '{')
                        currentchar = GetNextByte();
                
                break;

        }
    }

    f_close(&in);
}