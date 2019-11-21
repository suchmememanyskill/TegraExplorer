#include "../gfx/gfx.h"
#include "te.h"
#include "../utils/btn.h"
#include "gfx.h"

void clearscreen(){
    gfx_clear_grey(0x1B);
    gfx_box(0, 0, 719, 15, COLOR_WHITE);
    gfx_con_setpos(0, 0);
    gfx_printf("%k%KTegraexplorer%k%K\n", COLOR_DEFAULT, COLOR_WHITE, COLOR_WHITE, COLOR_DEFAULT);
}

int message(char* message, u32 color){
    clearscreen();
    gfx_printf("%k%s%k", color, message, COLOR_DEFAULT);
    return btn_wait();
}

int makemenu(menu_item menu[], int menuamount){
    int currentpos = 1, i, res;
    clearscreen();

    while (1){
        gfx_con_setpos(0, 31);

        if (currentpos == 1){
            while (currentpos < menuamount && menu[currentpos - 1].property < 0)
                currentpos++;
        }
        if (currentpos == menuamount){
            while (currentpos > 1 && menu[currentpos - 1].property < 0)
                currentpos--;
        }

        for (i = 0; i < menuamount; i++){
            if (menu[i].property < 0)
                continue;
            if (i == currentpos - 1)
                gfx_printf("%k%K%s%K\n", COLOR_DEFAULT, COLOR_WHITE, menu[i].name, COLOR_DEFAULT);
            else
                gfx_printf("%k%s\n", menu[i].color, menu[i].name);
        }
        gfx_printf("\n%k%s", COLOR_WHITE, menuamount);

        res = btn_wait();

        if (res & BTN_VOL_UP && currentpos > 1){
            currentpos--;
            while(menu[currentpos - 1].property < 0 && currentpos > 1)
                currentpos--;
        }
            
        else if (res & BTN_VOL_DOWN && currentpos < menuamount){
            currentpos++;
            while(menu[currentpos - 1].property < 0 && currentpos < menuamount)
                currentpos++;
        }

        else if (res & BTN_POWER)
            return menu[currentpos - 1].internal_function;
    }
}