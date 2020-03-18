#include <string.h>
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../emmc.h"
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

#include <stdlib.h>

char func[11] = "", args[2][128] = {"", ""};
int res, errcode;
const int scriptver = 133;
bool forceExit = false;
u32 currentcolor;

void Part_CheckFile(){
    FILINFO fno;
    errcode = f_stat(args[0], &fno);
}

void Part_SetColor(){
    if (strcmpcheck(args[0], "RED"))
        currentcolor = COLOR_RED;
    else if (strcmpcheck(args[0], "ORANGE"))
        currentcolor = COLOR_ORANGE;
    else if (strcmpcheck(args[0], "YELLOW"))
        currentcolor = COLOR_YELLOW;
    else if (strcmpcheck(args[0], "GREEN"))
        currentcolor = COLOR_GREEN;
    else if (strcmpcheck(args[0], "BLUE"))
        currentcolor = COLOR_BLUE;
    else if (strcmpcheck(args[0], "VIOLET"))
        currentcolor = COLOR_VIOLET;
    else if (strcmpcheck(args[0], "WHITE"))
        currentcolor = COLOR_WHITE;
}

void Part_Wait(){
    int waitamount, begintime;
    SWAPCOLOR(currentcolor);

    waitamount = atoi(args[0]);
    begintime = get_tmr_s();

    while (begintime + waitamount > get_tmr_s()){
        gfx_printf("\r<Wait %d seconds> ", (begintime + waitamount) - get_tmr_s());
    }

    gfx_printf("\r                 \r");
}

void Part_VersionCheck(){
    int givenversion = atoi(args[0]);

    if (givenversion > scriptver){
        gfx_printf("Script required version is too high\nUpdate TegraExplorer!");
        btn_wait();
        forceExit = true;
    }
}

void Part_Move(){
    errcode = f_rename(args[0], args[1]);
    if (errcode)
        f_rename(args[0], args[1]);
}

void Part_Delete(){
    errcode = f_unlink(args[0]);
    if (errcode)
        f_unlink(args[0]);
}

void Part_DeleteRecursive(){
    errcode = fsact_del_recursive(args[0]);
}

void Part_Copy(){
    errcode = fsact_copy(args[0], args[1], COPY_MODE_PRINT); 
}

void Part_RecursiveCopy(){
    errcode = fsact_copy_recursive(args[0], args[1]);
}

void Part_MakeFolder(){
    errcode = f_mkdir(args[0]);
    if (errcode)
        f_mkdir(args[0]);
}

void Part_ConnectMMC(){
    if (strcmpcheck(args[0], "SYSMMC"))
        connect_mmc(SYSMMC);
    if (strcmpcheck(args[0], "EMUMMC"))
        connect_mmc(EMUMMC);
}

void Part_MountMMC(){
    errcode = mount_mmc(args[0], 2);
}

void Part_Print(){
    RESETCOLOR;
    SWAPCOLOR(currentcolor);
    gfx_printf("%s\n", args[0]);
}

void Part_ErrorPrint(){
    RESETCOLOR;
    SWAPCOLOR(COLOR_RED);
    gfx_printf("Errorcode: %d\n", errcode);
}

void Part_Exit(){
    forceExit = true;
}

u8 buttons_pressed = 0;
void Part_WaitOnUser(){
    buttons_pressed = btn_wait();
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
    {"MOVE", Part_Move, 2},
    {"VERSION", Part_VersionCheck, 1},
    {"WAIT", Part_Wait, 1},
    {"COLOR", Part_SetColor, 1},
    {"CHECKPATH", Part_CheckFile, 1},
    {"NULL", NULL, -1}
};

int ParsePart(){
    int i;
    for (i = 0; parts[i].arg_amount != -1; i++){
        if (strcmpcheck(func, parts[i].name))
            return i;
    }
    gfx_printf("Parsing error...\nPress any key to continue");
    btn_wait();
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
    currentcolor = COLOR_WHITE;
    
    gfx_clearscreen();

    res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        gfx_errDisplay("ParseScript", res, 1);
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
                parts[res].handler();
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
                    inifstatement = (errcode);
                }
                else if (strcmpcheck(func, "NOERROR") || strcmpcheck(func, "FALSE")){
                    inifstatement = (!errcode);
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