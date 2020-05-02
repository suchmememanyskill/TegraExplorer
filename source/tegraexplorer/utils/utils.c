#include <string.h>
#include "utils.h"
#include "../common/common.h"
#include "../gfx/menu.h"
#include "../../storage/emummc.h"
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../../hid/hid.h"
/*
#include "../../utils/util.h"
#include "../../utils/sprintf.h"
#include "../../libs/fatfs/ff.h"
#include "../fs/fsutils.h"
*/

int utils_mmcMenu(){
    if (emu_cfg.enabled)
        return menu_make(utils_mmcChoice, 3, "-- Choose MMC --");    
    else
        return SYSMMC;
}

void utils_copystring(const char *in, char **out){
    int len = strlen(in) + 1;
    *out = (char *) malloc (len);
    strcpy(*out, in);
}

/*
void utils_takeScreenshot(){
    char *name, *path;
    char basepath[] = "sd:/tegraexplorer/screenshots";
    name = malloc(35);
    sprintf(name, "Screenshot_%d", get_tmr_s());

    f_mkdir("sd:/tegraexplorer");
    f_mkdir(basepath);
    path = fsutil_getnextloc(basepath, name);
}
*/

char *utils_InputText(char *start, int maxLen){
    int offset = -1, currentPos = 0, len;
    char temp;
    Inputs *input = hidRead();
    u32 x, y;
    gfx_printf("Add characters by pressing X\nRemove characters by pressing Y\nJoysticks for movement\nB to cancel, A to accept\n\n");
    gfx_con_getpos(&x, &y);

    if (strlen(start) > maxLen)
        return NULL;

    char *buff;
    buff = calloc(maxLen + 1, sizeof(char));
    strcpy(buff, start);

    while (1){
        offset = -1;
        gfx_con_setpos(x, y);
        for (int i = 0; i < 3; i++){
            if (offset != 0){
                SWAPCOLOR(0xFF666666);
                gfx_con.fntsz = 8;
            }
            else {
                SWAPCOLOR(COLOR_WHITE);
                gfx_con.fntsz = 16;
            }
            for (int x = 0; x < strlen(buff); x++){
                if (offset == 0 && x == currentPos){
                    gfx_printf("%k%c%k", COLOR_GREEN, buff[x], COLOR_WHITE);
                }
                else {
                    temp = buff[x] + offset;

                    if (!(temp >= 32 && temp <= 126))
                        temp = ' ';

                    gfx_putc(temp);
                }

                if (offset != 0)
                    gfx_puts("   ");
                else
                    gfx_putc(' ');
            }
            gfx_putc('\n');
            offset++;
        }

        if (input->buttons & (KEY_RDOWN | KEY_RUP))
            hidRead();
        else
            hidWait();

        len = strlen(buff);

        if (input->buttons & (KEY_A | KEY_B))
            break;

        if (input->buttons & (KEY_LDOWN | KEY_RDOWN) && buff[currentPos] < 126)
            buff[currentPos]++;
        
        if (input->buttons & (KEY_LUP | KEY_RUP) && buff[currentPos] > 32)
            buff[currentPos]--;

        if (input->Lleft && currentPos > 0)
            currentPos--;

        if (input->Lright && currentPos < len - 1)
            currentPos++;
            
        if (input->x && maxLen > len){
            buff[len] = '.';
            buff[len + 1] = '\0';
        }

        if (input->y && len > 1){
            buff[len - 1] = '\0';
            if (currentPos == len - 1){
                currentPos--;
            }
            gfx_boxGrey(0, y, 1279, y + 48, 0x1B);
        }
    }

    gfx_con.fntsz = 16;

    if (input->b){
        free(buff);
        return NULL;
    }

    return buff;
}