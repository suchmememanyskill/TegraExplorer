#include <string.h>
#include "menu.h"
#include "gfxutils.h"
#include "../common/types.h"
#include "../../utils/btn.h"
#include "../common/common.h"
#include "../../utils/util.h"
#include "../../mem/minerva.h"
#include "../../soc/gpio.h"
#include "../../hid/hid.h"
#include "../fs/fsutils.h"
#include "../utils/menuUtils.h"

extern void sd_unmount();
extern bool sd_inited;

#pragma GCC push_options
#pragma GCC optimize ("O2")

void _printentry(menu_entry *entry, bool highlighted, bool refresh, const char *path){
    if (entry->isHide)
        return;

    u32 color = (entry->isMenu) ? entry->storage : ((entry->isDir) ? COLOR_WHITE : COLOR_VIOLET);

    SWAPALLCOLOR((highlighted) ? COLOR_DEFAULT : color, (highlighted) ? color : COLOR_DEFAULT);

    if (!(entry->isMenu))
        gfx_printf("%c ", (entry->isDir) ? 30 : 31);

    if (refresh)
        gfx_printandclear(entry->name, 37, 720);
    else
        gfx_printlength(37, entry->name);

    if (entry->property & (ISMENU | ISDIR))
        gfx_printf("\n");
    else { 
        if (entry->isNull){
            u64 totalSize;
            u32 sizeType = 0;
            totalSize = fsutil_getfilesize(fsutil_getnextloc(path, entry->name));
        
            while (totalSize > 1024){
                totalSize /= 1024;
                sizeType++;
            }

            if (sizeType > 3)
                sizeType = 3;

            entry->size = sizeType;
            entry->storage = totalSize;
            SETBIT(entry->property, ISNULL, 0);
        }

        SWAPALLCOLOR(COLOR_BLUE, COLOR_DEFAULT);
        gfx_printf("\a%4d", entry->storage);
        gfx_con.fntsz = 8;
        gfx_printf("\n\e%s\n", gfx_file_size_names[entry->size]);
        gfx_con.fntsz = 16;
    }   
}


bool disableB = false;
int menu_make(menu_entry *entries, int amount, const char *toptext){
    int currentpos = 0, offset = 0, delay = 300, minscreen = 0, calculatedamount = 0, cursub = 0, temp;
    u32 scrolltimer, timer, sideY;
    bool refresh = true;
    Inputs *input = hidRead();
    input->buttons = 0;

    gfx_clearscreen();

    calculatedamount = mu_countObjects(entries, amount, ISMENU);

    gfx_con_setpos(0, 16);

    SWAPCOLOR(COLOR_GREEN);
    gfx_printlength(42, toptext);
    RESETCOLOR;

    gfx_sideSetY(48);

    char *currentfolder = strrchr(toptext, '/');
    if (currentfolder != NULL){
        if (calculatedamount)
            gfx_sideprintf("%d items in current dir\n\n", calculatedamount);

        gfx_sideprintf("Current directory:\n");

        if (*(currentfolder + 1) != 0)
            currentfolder++;
        SWAPCOLOR(COLOR_GREEN);
        gfx_sideprintandclear(currentfolder, 28);

        gfx_sideprintf("\n\n\n");
    }

    for (;;){
        gfx_con_setpos(0, 48);
        timer = get_tmr_ms();

        if (!currentpos)
            cursub = 1;
        else if (currentpos == amount - 1)
            cursub = -1;
        else
            cursub = 0;
            
        while (entries[currentpos].property & (ISSKIP | ISHIDE))
                currentpos += cursub;
        

        if (currentpos > (minscreen + SCREENMAXOFFSET)){
            offset += currentpos - (minscreen + SCREENMAXOFFSET);
            minscreen += currentpos - (minscreen + SCREENMAXOFFSET);
            refresh = true;
        }
        else if (currentpos < minscreen){
            offset -= minscreen - currentpos;
            minscreen -= minscreen - currentpos;  
            refresh = true;    
        }
            

        if (refresh || currentfolder == NULL || !calculatedamount){
            for (int i = 0 + offset; i < amount && i < SCREENMAXOFFSET + 1 + offset; i++)
                _printentry(&entries[i], (i == currentpos), refresh, toptext);
        }
        else {
            temp = (currentpos - minscreen > 0) ? 0 : 16;
            gfx_con_setpos(0, 32 + temp + (currentpos - minscreen) * 16);

            if (!temp)
                _printentry(&entries[currentpos - 1], false, false, toptext);

            _printentry(&entries[currentpos], true, false, toptext);

            if (currentpos < MIN(amount - 1, minscreen + SCREENMAXOFFSET))
                _printentry(&entries[currentpos + 1], false, false, toptext);
        }

        RESETCOLOR;


        sideY = gfx_sideGetY();
        if (!(entries[currentpos].isMenu)){
            gfx_sideprintf("Current selection:\n");
            SWAPCOLOR(COLOR_YELLOW);
            gfx_sideprintandclear(entries[currentpos].name, 28);
            RESETCOLOR;
            gfx_sideprintf("Type: %s", (entries[currentpos].isDir) ? "Dir " : "File");
            gfx_sideSetY(sideY);
        }
        else
            gfx_boxGrey(800, sideY, 1279, sideY + 48, 0x1B);

        gfx_con_setpos(0, 703);
        SWAPALLCOLOR(COLOR_DEFAULT, COLOR_WHITE);
        gfx_printf("Time taken for screen draw: %dms  ", get_tmr_ms() - timer);

        if (refresh)
            gfx_drawScrollBar(minscreen, minscreen + SCREENMAXOFFSET, amount);

        refresh = false;


        while (hidRead()->buttons & (KEY_B | KEY_A));

        scrolltimer = get_tmr_ms();
        while (1){
            if (sd_inited && !!gpio_read(GPIO_PORT_Z, GPIO_PIN_1)){
                gfx_errDisplay("menu", ERR_SD_EJECTED, 0);
                sd_unmount();
                return -1;
            }

            input = hidRead();

            if (!(input->buttons & (KEY_A | KEY_LDOWN | KEY_LUP | KEY_B | KEY_RUP | KEY_RDOWN))){
                delay = 300;
                continue;
            }
            
            if (input->buttons & (KEY_RDOWN | KEY_RUP)){
                delay = 1;
                input->Lup = input->Rup;
                input->Ldown = input->Rdown;
            }

            if (delay < 300){
                if (scrolltimer + delay < get_tmr_ms()){
                    break;
                }
            }
            else {
                break;
            }
        }


        if (input->a){
            break;
        }   
        if (input->b && !disableB){
            currentpos = 0;
            break;
        }


        if (delay > 46)
            delay -= 45;


        if (input->Lup && currentpos > 0)
            cursub = -1;
        else if (input->Ldown && currentpos < amount - 1)
            cursub = 1;
        else
            cursub = 0;

        if (cursub){
            do {
                currentpos += cursub;
            } while (currentpos < amount - 1 && currentpos > 0 && entries[currentpos].property & (ISSKIP | ISHIDE));
        }
    }

    minerva_periodic_training();
    //return (mode) ? currentpos : entries[currentpos].property;
    return currentpos;
}

#pragma GCC pop_options