#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "utils.h"

void utils_gfx_init(){
    display_backlight_brightness(100, 1000);
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
}

void utils_waitforpower(){
    u32 btn = btn_wait();
    if (btn & BTN_VOL_UP)
        reboot_rcm();
    else if (btn & BTN_VOL_DOWN)
        reboot_normal();
    else
        power_off();
}