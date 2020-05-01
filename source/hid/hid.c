#include "hid.h"
#include "joycon.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"
#include "../utils/types.h"

static Inputs inputs = {0};
u16 LbaseX = 0, LbaseY = 0, RbaseX = 0, RbaseY = 0;

void hidInit(){
    jc_init_hw();
}

Inputs *hidRead(){
    jc_gamepad_rpt_t *controller = joycon_poll();
    static bool errPrint = false;

    u8 btn = btn_read();
    inputs.volp = (btn & BTN_VOL_UP) ? 1 : 0;
    inputs.volm = (btn & BTN_VOL_DOWN) ? 1 : 0;
    inputs.pow = (btn & BTN_POWER) ? 1 : 0;

    inputs.a = controller->a;
    inputs.b = controller->b;
    inputs.x = controller->x;
    inputs.y = controller->y;
    inputs.r = controller->r;
    inputs.l = controller->l;
    inputs.zr = controller->zr;
    inputs.zl = controller->zl;
    inputs.minus = controller->minus;
    inputs.plus = controller->plus;
    inputs.home = controller->home;
    inputs.cap = controller->cap;

    if (controller->conn_l && controller->conn_r){
        if (errPrint){
            gfx_boxGrey(1008, 703, 1279, 719, 0xFF);
            errPrint = false;
        }
    }
    else {
        gfx_con_setpos(1008, 703);
        gfx_printf("%k%K%s %s MISS%k%K", COLOR_DEFAULT, COLOR_WHITE, (controller->conn_l) ? "    " : "JOYL", (controller->conn_r) ? "    " : "JOYR", COLOR_WHITE, COLOR_DEFAULT);
        errPrint = true;
    }

    if (controller->conn_l){
        if ((LbaseX == 0 || LbaseY == 0) || controller->l3){
            LbaseX = controller->lstick_x;
            LbaseY = controller->lstick_y;
        }

        inputs.Lup = (controller->up || (controller->lstick_y > LbaseY + 500)) ? 1 : 0;
        inputs.Ldown = (controller->down || (controller->lstick_y < LbaseY - 500)) ? 1 : 0;
        inputs.Lleft = (controller->left || (controller->lstick_x < LbaseX - 500)) ? 1 : 0;
        inputs.Lright = (controller->right || (controller->lstick_x > LbaseX + 500)) ? 1 : 0;
    }
    else {
        inputs.Lup = inputs.volp;
        inputs.Ldown = inputs.volm;
    }

    if (controller->conn_r){
        if ((RbaseX == 0 || RbaseY == 0) || controller->r3){
            RbaseX = controller->rstick_x;
            RbaseY = controller->rstick_y;
        }

        inputs.Rup = (controller->rstick_y > RbaseY + 500) ? 1 : 0;
        inputs.Rdown = (controller->rstick_y < RbaseY - 500) ? 1 : 0;
        inputs.Rleft = (controller->rstick_x < RbaseX - 500) ? 1 : 0;
        inputs.Rright = (controller->rstick_x > RbaseX + 500) ? 1 : 0;
    }
    else
        inputs.a = inputs.pow;

    return &inputs;
}

Inputs *hidWaitMask(u32 mask){
    Inputs *in = hidRead();
    while ((in->buttons & mask) == 0){
        in = hidRead();
    }
    return in;
}

Inputs *hidWait(){
    Inputs *in = hidRead();

    while (in->buttons)
        hidRead();

    while (!(in->buttons))
        hidRead();
    return in;
}