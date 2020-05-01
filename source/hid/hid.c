#include "hid.h"
#include "joycon.h"
#include "../utils/btn.h"

static Inputs inputs = {0};
u16 LbaseX = 0, LbaseY = 0, RbaseX = 0, RbaseY = 0;

void hidInit(){
    jc_init_hw();
}

Inputs *hidRead(){
    jc_gamepad_rpt_t *controller = joycon_poll();

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

    u8 btn = btn_read();
    inputs.volp = (btn & BTN_VOL_UP) ? 1 : 0;
    inputs.volm = (btn & BTN_VOL_DOWN) ? 1 : 0;
    inputs.pow = (btn & BTN_POWER) ? 1 : 0;

    return &inputs;
}

Inputs *hidWaitForButton(u32 mask){
    Inputs *in = hidRead();
    while ((in->buttons & mask) == 0){
        in = hidRead();
    }
    return in;
}