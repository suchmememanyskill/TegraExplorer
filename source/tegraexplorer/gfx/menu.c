#include "menu.h"
#include "gfxutils.h"
#include "../common/types.h"
#include "../../utils/btn.h"
#include "../common/common.h"
#include "../../utils/util.h"
#include "../../mem/minerva.h"

void _printentry(menu_entry entry, bool highlighted, bool refresh){
    int size;

    if (entry.property & ISMENU)
        SWAPCOLOR(entry.storage);
    else if (entry.property & ISDIR)
        SWAPCOLOR(COLOR_WHITE);
    else {
        SWAPCOLOR(COLOR_VIOLET);

        for (size = 4; size < 8; size++)
            if ((entry.property & (1 << size)))
                break;
    }
        

    if (highlighted){
        SWAPBGCOLOR(COLOR_WHITE);
        if ((entry.property & ISMENU) ? entry.storage == COLOR_WHITE : entry.property & ISDIR)
            SWAPCOLOR(COLOR_DEFAULT);
    }
    else
        SWAPBGCOLOR(COLOR_DEFAULT);
        

    if (refresh)
        gfx_printandclear(entry.name, 37);
    else
        gfx_printlength(37, entry.name);

    if (entry.property & ISDIR || entry.property & ISMENU)
        gfx_printf("\n");
    else { 
        SWAPCOLOR(COLOR_BLUE);
        gfx_printf("\a%d\e%s", entry.storage, gfx_file_size_names[size - 4]);
    }   
}

int menu_make(menu_entry *entries, int amount, char *toptext){
    int currentpos = 0, i, res = 0, offset = 0, delay = 300;
    u32 scrolltimer, timer;
    bool refresh = false;

    gfx_clearscreen();

    gfx_con_setpos(512, 0);
    SWAPCOLOR(COLOR_DEFAULT);
    SWAPBGCOLOR(COLOR_WHITE);
    gfx_printf("%3d entries\n", amount);
    RESETCOLOR;

    SWAPCOLOR(COLOR_GREEN);
    gfx_printlength(42, toptext);
    RESETCOLOR;

    while (!(res & BTN_POWER)){
        gfx_con_setpos(0, 47);
        timer = get_tmr_ms();

        if (!currentpos){
            while (currentpos < amount && entries[currentpos].property & (ISSKIP | ISHIDE))
                currentpos++;
        }
        if (currentpos == amount - 1){
            while (currentpos >= 1 && entries[currentpos].property & (ISSKIP | ISHIDE))
                currentpos--;
        }

        for (int i = 0 + offset; i < amount && i < 60 + offset; i++)
            if (!(entries[i].property & ISHIDE))
                _printentry(entries[i], (i == currentpos), refresh);

        gfx_printf("\n%k%K %s %s\n\nTime taken for screen draw: %dms ", COLOR_BLUE, COLOR_DEFAULT, (offset + 60 < amount) ? "v" : " ", (offset > 0) ? "^" : " ", get_tmr_ms() - timer);

        while (btn_read() & BTN_POWER);

        res = 0;
        while (!res){
            res = btn_read();

            if (!res)
                delay = 300;
            
            if (delay < 300){
                scrolltimer = get_tmr_ms();
                while (res){
                    if (scrolltimer + delay <= get_tmr_ms())
                        break;

                    res = btn_read();         
                }
            }

            if (delay > 46 && res)
                delay -= 45;
        }

        if (res & BTN_VOL_UP && currentpos >= 1){
            currentpos--;
            while(entries[currentpos].property & (ISSKIP | ISHIDE) && currentpos >= 1)
                currentpos--;
        }
            
        else if (res & BTN_VOL_DOWN && currentpos < amount - 1){
            currentpos++;
            while(entries[currentpos].property & (ISSKIP | ISHIDE) && currentpos < amount - 1)
                currentpos++;
        }

    }

    minerva_periodic_training();
    //return (mode) ? currentpos : entries[currentpos].property;
    return currentpos;
}