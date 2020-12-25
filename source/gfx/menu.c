#include "menu.h"
#include "../utils/vector.h"
#include "gfx.h"
#include "gfxutils.h"
#include "../hid/hid.h"
#include <utils/util.h>
#include <utils/btn.h>
#include <utils/sprintf.h>
#include <string.h>

const char *sizeDefs[] = {
    "B ",
    "KB",
    "MB",
    "GB"
};

void _printEntry(MenuEntry_t entry, u32 maxLen, u8 highlighted, u32 bg){
    if (entry.hide)
        return;

    (highlighted) ? SETCOLOR(bg, RGBUnionToU32(entry.optionUnion)) : SETCOLOR(RGBUnionToU32(entry.optionUnion), bg);
    
    if (entry.icon){
        gfx_putc(entry.icon);
        gfx_putc(' ');
        maxLen -= 2;
    }
    
    u32 curX = 0, curY = 0;
    gfx_con_getpos(&curX, &curY);
    gfx_puts_limit(entry.name, maxLen - ((entry.showSize) ? 8 : 0));
    if (entry.showSize){
        (highlighted) ? SETCOLOR(bg, COLOR_BLUE) : SETCOLOR(COLOR_BLUE, bg);
        gfx_con_setpos(curX + (maxLen - 6) * 16, curY);
        gfx_printf("%4d", entry.size);
        gfx_puts_small(sizeDefs[entry.sizeDef]);
    }

    gfx_putc('\n');
}

void FunctionMenuHandler(MenuEntry_t *entries, int entryCount, menuPaths *paths, u8 options){
    Vector_t ent = vecFromArray(entries, entryCount, sizeof(MenuEntry_t));
    gfx_clearscreen();
    gfx_putc('\n');
    int res = newMenu(&ent, 0, 79, 30, options, entryCount);
    if (paths[res] != NULL)
        paths[res]();
}

int newMenu(Vector_t* vec, int startIndex, int screenLenX, int screenLenY, u8 options, int entryCount) {
    vecPDefArray(MenuEntry_t*, entries, vec);
    u32 selected = startIndex;

    while (entries[selected].skip || entries[selected].hide)
        selected++;

    u32 lastIndex = selected;
    u32 startX = 0, startY = 0;
    gfx_con_getpos(&startX, &startY);

    u32 bgColor = (options & USELIGHTGREY) ? COLOR_DARKGREY : COLOR_DEFAULT;

    /*
    if (options & ENABLEPAGECOUNT){
        screenLenY -= 2;
        startY += 32;
    }
    */

    bool redrawScreen = true;
    Input_t *input = hidRead();

    // Maybe add a check here so you don't read OOB by providing a too high startindex?

    u32 lastPress = 0x666 + get_tmr_ms();
    u32 holdTimer = 300;

    while(1) {
        u32 lastDraw = get_tmr_ms();
        if (redrawScreen || options & ALWAYSREDRAW){
            if (options & ENABLEPAGECOUNT){
                SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
                char temp[40] = "";
                sprintf(temp, " Page %d / %d | Total %d entries", (selected / screenLenY) + 1, (vec->count / screenLenY) + 1, entryCount);
                gfx_con_setpos(YLEFT - strlen(temp) * 18, 0);
                gfx_printf(temp);
            }

            gfx_con_setpos(startX, startY);

            if (redrawScreen)
                gfx_boxGrey(startX, startY, startX + screenLenX * 16, startY + screenLenY * 16, (options & USELIGHTGREY) ? 0x33 : 0x1B);

            int start = selected / screenLenY * screenLenY;
            for (int i = start; i < MIN(vec->count, start + screenLenY); i++){
                gfx_con_setpos(startX, startY + ((i % screenLenY) * 16));
                _printEntry(entries[i], screenLenX, (i == selected), bgColor);
            }
                
        } 
        else if (lastIndex != selected) {
            u32 minLastCur = MIN(lastIndex, selected);
            u32 maxLastCur = MAX(lastIndex, selected);
            gfx_con_setpos(startX, startY + ((minLastCur % screenLenY) * 16));
            _printEntry(entries[minLastCur], screenLenX, (minLastCur == selected), bgColor);
            gfx_con_setpos(startX, startY + ((maxLastCur % screenLenY) * 16));
            _printEntry(entries[maxLastCur], screenLenX, (minLastCur != selected), bgColor);
        }

        lastIndex = selected;

        SETCOLOR(COLOR_DEFAULT, COLOR_WHITE);
        gfx_con_setpos(0, 704);
        gfx_printf("Time taken for screen draw: %dms  ", get_tmr_ms() - lastDraw);
        
        while(hidRead()){
            if (!(input->buttons)){
                holdTimer = 300;
                break;
            }

            if (input->buttons & (JoyRUp | JoyRDown))
                holdTimer = 40;

            if ((lastPress + holdTimer) < get_tmr_ms()){
                if (holdTimer > 50)
                    holdTimer -= 50;
                break;
            }
        }
       
        while (1){
            if (hidRead()->a)
                return selected;
            else if (input->b && options & ENABLEB)
                return 0;
            else if (input->down || input->rDown || input->right){ //Rdown should probs not trigger a page change. Same for RUp
                u32 temp = (input->right && !(input->down || input->rDown)) ? screenLenY : 1;

                if (vec->count > selected + temp){
                    selected += temp;
                    break;
                }
                else if (input->right && (selected / screenLenY != (vec->count - 1) / screenLenY)){
                    selected = vec->count - 1;
                    break;
                }
            }
            else if (input->up || input->rUp || input->left){
                u32 temp = (input->left && !(input->up || input->rUp)) ? screenLenY : 1;
                if (selected >= temp){
                    selected -= temp;
                    break;
                }
            }
            else
                holdTimer = 300;
        }

        lastPress = get_tmr_ms();

        int m = (selected > lastIndex) ? 1 : -1;
        while (selected > 0 && selected < vec->count - 1 && entries[selected].optionUnion & SKIPHIDEBITS)
            selected += m;

        if (entries[selected].optionUnion & SKIPHIDEBITS)
            selected = lastIndex;

        redrawScreen = (selected / screenLenY != lastIndex / screenLenY);
    }
}