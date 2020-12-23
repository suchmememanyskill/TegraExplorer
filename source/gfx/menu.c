#include "menu.h"
#include "../utils/vector.h"
#include "gfx.h"
#include "gfxutils.h"
#include "../hid/hid.h"
#include <utils/util.h>
#include <utils/btn.h>

const char *sizeDefs[] = {
    "B ",
    "KB",
    "MB",
    "GB"
};

void _printEntry(MenuEntry_t entry, u32 maxLen, u8 highlighted){
    (highlighted) ? SETCOLOR(COLOR_DEFAULT, FromRGBtoU32(entry.R, entry.G, entry.B)) : SETCOLOR(FromRGBtoU32(entry.R, entry.G, entry.B), COLOR_DEFAULT);
    
    if (entry.icon){
        gfx_putc(entry.icon);
        maxLen--;
    }
    
    u32 curX = 0, curY = 0;
    gfx_con_getpos(&curX, &curY);
    gfx_puts_limit(entry.name, maxLen - 8);
    if (entry.showSize){
        gfx_con_setpos(curX + (maxLen - 6) * 16, curY);
        gfx_printf("%d", entry.size);
        gfx_puts_small(sizeDefs[entry.sizeDef]);
    }

    gfx_putc('\n');
}

void FunctionMenuHandler(MenuEntry_t *entries, int entryCount, menuPaths *paths, bool enableB){
    Vector_t ent = vecFromArray(entries, entryCount, sizeof(MenuEntry_t));
    gfx_clearscreen();
    gfx_putc('\n');
    int res = newMenu(&ent, 0, 79, 30, NULL, NULL, enableB, entryCount);
    if (paths[res] != NULL)
        paths[res]();
}

int newMenu(Vector_t* vec, int startIndex, int screenLenX, int screenLenY, menuEntriesGatherer gatherer, void *ctx, bool enableB, int totalEntries) {
    vecPDefArray(MenuEntry_t*, entries, vec);
    u32 selected = startIndex;

    while (entries[selected].skip || entries[selected].hide)
        selected++;

    u32 lastIndex = selected;
    u32 startX = 0, startY = 0;
    gfx_con_getpos(&startX, &startY);
    if (totalEntries > screenLenY)
        startY += 16;
    bool redrawScreen = true;

    Input_t *input = hidRead();

    // Maybe add a check here so you don't read OOB by providing a too high startindex?

    //u32 lastPress = 0xFFFF3FFF;
    //u32 holdTimer = 500;

    while(1) {
        if (redrawScreen){
            if (totalEntries > screenLenY){
                gfx_con_setpos(startX, startY - 16);
                RESETCOLOR;
                gfx_printf("Page %d / %d ", (selected / screenLenY) + 1, (totalEntries / screenLenY) + 1);
            }
            gfx_con_setpos(startX, startY);
            gfx_boxGrey(startX, startY, startX + screenLenX * 16, startY + screenLenY * 16, 0x1B);
            int start = selected / screenLenY * screenLenY;
            for (int i = start; i < MIN(vec->count, start + screenLenY); i++)
                _printEntry(entries[i], screenLenX, (i == selected));
        } 
        else if (lastIndex != selected) {
            u32 minLastCur = MIN(lastIndex, selected);
            gfx_con_setpos(startX, startY + ((minLastCur % screenLenY) * 16));
            _printEntry(entries[minLastCur], screenLenX, (minLastCur == selected));
            _printEntry(entries[minLastCur + 1], screenLenX, (minLastCur != selected));
        }

        lastIndex = selected;

        /*
        // i don't know what fuckery goes on here but this doesn't work. Will fix this later
        while (1){
            hidRead();
            
            if (!(input->buttons & WAITBUTTONS)){
                lastPress = 0;
                holdTimer = 500;
                break;
            }
            

            
            if ((lastPress + holdTimer) < get_tmr_ms()){
                lastPress = get_tmr_ms();
                //if (holdTimer > 50)
                //    holdTimer -= 50;
                break;
            }
            
        }
        */
        

       
        while (hidRead()->buttons & WAITBUTTONS);
        while (1){
            if (hidRead()->a)
                return selected;
            else if (input->b && enableB)
                return 0;
            else if (input->down || input->rDown || input->right){ //Rdown should probs not trigger a page change. Same for RUp
                u32 temp = (input->right) ? screenLenY : 1;
                if (vec->count <= selected + temp){
                    if (gatherer != NULL){
                        gatherer(vec, ctx);
                        entries = vecPGetArray(MenuEntry_t*, vec);
                    }
                }

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
                u32 temp = (input->left) ? screenLenY : 1;
                if (selected >= temp){
                    selected -= temp;
                    break;
                }
            }
        }

        int m = (selected > lastIndex) ? 1 : -1;
        while (selected > 0 && selected < vec->count - 1 && entries[selected].optionUnion & SKIPHIDEBITS)
            selected += m;

        if (entries[selected].optionUnion & SKIPHIDEBITS)
            selected = lastIndex;

        redrawScreen = (selected / screenLenY != lastIndex / screenLenY);
    }
}