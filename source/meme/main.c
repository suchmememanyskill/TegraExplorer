#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "main.h"

void meme_main(){
    utils_gfx_init();
    static const u32 colors[7] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT};
    gfx_printf("%k%pHello World!\n%k%pHi denn i think i did it\n%p%kAnother test", colors[1], colors[0], colors[2], colors[5], colors[6], colors[3]);
    utils_waitforpower();
}