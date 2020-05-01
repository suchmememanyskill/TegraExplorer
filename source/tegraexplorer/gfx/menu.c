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

extern void sd_unmount();
extern bool sd_inited;

void _printentry(menu_entry entry, bool highlighted, bool refresh){
    int size;
    u32 color = (entry.property & ISMENU) ? entry.storage : ((entry.property & ISDIR) ? COLOR_WHITE : COLOR_VIOLET);
    /*
    if (entry.property & ISMENU)
        SWAPCOLOR(entry.storage);
    else if (entry.property & ISDIR)
        SWAPCOLOR(COLOR_WHITE);
    else {
        SWAPCOLOR(COLOR_VIOLET);
    }
    */

   if (!(entry.property & ISMENU && entry.property & ISDIR)){
        for (size = 4; size < 8; size++)
            if ((entry.property & (1 << size)))
                break;
   }

        
    /*
    if (highlighted){
        SWAPBGCOLOR(COLOR_WHITE);
        if ((entry.property & ISMENU) ? entry.storage == COLOR_WHITE : entry.property & ISDIR)
            SWAPCOLOR(COLOR_DEFAULT);
    }
    else
        SWAPBGCOLOR(COLOR_DEFAULT);
    */
   SWAPCOLOR((highlighted) ? COLOR_DEFAULT : color);
   SWAPBGCOLOR((highlighted) ? color : COLOR_DEFAULT);
        
    if (!(entry.property & ISMENU))
        gfx_printf("%c ", (entry.property & ISDIR) ? 30 : 31);

    if (refresh)
        gfx_printandclear(entry.name, 37, 720);
    else
        gfx_printlength(37, entry.name);

    if (entry.property & ISDIR || entry.property & ISMENU)
        gfx_printf("\n");
    else { 
        SWAPCOLOR(COLOR_BLUE);
        SWAPBGCOLOR(COLOR_DEFAULT);
        gfx_printf("\a%4d", entry.storage);
        gfx_con.fntsz = 8;
        gfx_printf("\n\e%s\n", gfx_file_size_names[size - 4]);
        gfx_con.fntsz = 16;
    }   
}


bool disableB = false;
int menu_make(menu_entry *entries, int amount, char *toptext){
    int currentpos = 0, offset = 0, delay = 300, minscreen = 0, maxscreen = 39, calculatedamount = 0;
    u32 scrolltimer, timer;
    bool refresh = false;
    Inputs *input = hidRead();
    input->buttons = 0;

    gfx_clearscreen();

    for (int i = 0; i < amount; i++)
        if (!(entries[i].property & ISMENU))
            calculatedamount++;

    gfx_con_setpos(0, 16);
    /*
    if (calculatedamount){
        SWAPCOLOR(COLOR_DEFAULT);
        SWAPBGCOLOR(COLOR_WHITE);
        gfx_printf("%3d entries\n", calculatedamount);
        RESETCOLOR;
    }
    else
        gfx_printf("\n");
    */
    


    SWAPCOLOR(COLOR_GREEN);
    gfx_printlength(42, toptext);
    RESETCOLOR;

    char *currentfolder = strrchr(toptext, '/');
    if (currentfolder != NULL){
        gfx_con_setpos(800, 48);
        if (calculatedamount)
            gfx_printf("%d items in curr. folder\n%j", calculatedamount, 800);
        gfx_printf("Current directory:\n%j", 800);
        if (*(currentfolder + 1) != 0)
            currentfolder++;
        SWAPCOLOR(COLOR_GREEN);
        gfx_printlength(28, currentfolder);
    }

    while (!(input->a)){
        gfx_con_setpos(0, 48);
        timer = get_tmr_ms();
        refresh = false;

        if (!currentpos){
            while (currentpos < amount && entries[currentpos].property & (ISSKIP | ISHIDE))
                currentpos++;
        }
        if (currentpos == amount - 1){
            while (currentpos >= 1 && entries[currentpos].property & (ISSKIP | ISHIDE))
                currentpos--;
        }

        if (currentpos > maxscreen){
            offset += currentpos - maxscreen;
            minscreen += currentpos - maxscreen;
            maxscreen += currentpos - maxscreen;
            refresh = true;
        }

        if (currentpos < minscreen){
            offset -= minscreen - currentpos;
            maxscreen -= minscreen - currentpos;
            minscreen -= minscreen - currentpos;      
            refresh = true;
        }

        for (int i = 0 + offset; i < amount && i < 40 + offset; i++)
            if (!(entries[i].property & ISHIDE))
                _printentry(entries[i], (i == currentpos), refresh);

        RESETCOLOR;

        if (!(entries[currentpos].property & ISMENU)){
            gfx_con_setpos(800, 144);
            gfx_printf("Current selection:\n%j", 800);
            SWAPCOLOR(COLOR_YELLOW);
            gfx_printandclear(entries[currentpos].name, 28, 1279);
            RESETCOLOR;
            gfx_con_setpos(800, 175);
            gfx_printf("Type: %s", (entries[currentpos].property & ISDIR) ? "Dir " : "File");
        }
        else
            gfx_boxGrey(800, 223, 1279, 271, 0x1B);

        gfx_con_setpos(0, 703);
        SWAPCOLOR(COLOR_DEFAULT);
        SWAPBGCOLOR(COLOR_WHITE);
        gfx_printf("%s %s | Time taken for screen draw: %dms  ", (offset + 40 < amount) ? "v" : " ", (offset > 0) ? "^" : " ", get_tmr_ms() - timer);

        while ((input = hidRead())->buttons & (KEY_B | KEY_A));

        input->buttons = 0;
        while (!(input->buttons & (KEY_A | KEY_LDOWN | KEY_LUP | KEY_B | KEY_RUP | KEY_RDOWN))){
            if (sd_inited && !!gpio_read(GPIO_PORT_Z, GPIO_PIN_1)){
                gfx_errDisplay("menu", ERR_SD_EJECTED, 0);
                sd_unmount();
                return -1;
            }

            input = hidRead();

            if (!(input->buttons & (KEY_A | KEY_LDOWN | KEY_LUP | KEY_B | KEY_RUP | KEY_RDOWN)))
                delay = 300;
            
            if (delay < 300){
                scrolltimer = get_tmr_ms();
                while (input->buttons & (KEY_A | KEY_LDOWN | KEY_LUP | KEY_B | KEY_RUP | KEY_RDOWN)){
                    if (scrolltimer + delay <= get_tmr_ms())
                        break;

                    input = hidRead();       
                }
            }

            if (delay > 46 && input->buttons & (KEY_A | KEY_LDOWN | KEY_LUP | KEY_B | KEY_RUP | KEY_RDOWN))
                delay -= 45;

            if (input->buttons & (KEY_RUP | KEY_RDOWN))
                delay = 1;
        }

        if (input->buttons & (KEY_LUP | KEY_RUP) && currentpos >= 1){
            currentpos--;
            while(entries[currentpos].property & (ISSKIP | ISHIDE) && currentpos >= 1)
                currentpos--;
        }
            
        else if (input->buttons & (KEY_LDOWN | KEY_RDOWN) && currentpos < amount - 1){
            currentpos++;
            while(entries[currentpos].property & (ISSKIP | ISHIDE) && currentpos < amount - 1)
                currentpos++;
        }

        else if (input->b && !disableB){
            currentpos = 0;
            break;
        }
    }

    minerva_periodic_training();
    //return (mode) ? currentpos : entries[currentpos].property;
    return currentpos;
}