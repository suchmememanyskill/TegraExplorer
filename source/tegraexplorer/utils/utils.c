#include <string.h>
#include "utils.h"
#include "../common/common.h"
#include "../gfx/menu.h"
#include "../../storage/emummc.h"
#include "../../mem/heap.h"

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