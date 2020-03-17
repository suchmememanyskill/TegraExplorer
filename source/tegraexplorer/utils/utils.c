#include "utils.h"
#include "../common/common.h"
#include "../gfx/menu.h"
#include "../../storage/emummc.h"

int utils_mmcMenu(){
    int res;

    if (emu_cfg.enabled)
        return menu_make(utils_mmcChoice, 3, "-- Choose MMC --");    
    else
        return SYSMMC;
}