#include <string.h>
#include "utils.h"
#include "../common/common.h"
#include "../gfx/menu.h"
#include "../../storage/emummc.h"
#include "../../mem/heap.h"
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