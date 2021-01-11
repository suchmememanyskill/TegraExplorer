#include "hid.h"
#include <input/joycon.h>
#include <utils/btn.h>
#include "../gfx/gfx.h"
#include <utils/types.h>
#include <utils/util.h>
#include "../utils/utils.h"
#include "../tegraexplorer/tools.h"
#include <display/di.h>
#include "../config.h"

static Input_t inputs = {0};
u16 LbaseX = 0, LbaseY = 0, RbaseX = 0, RbaseY = 0;

void hidInit(){
    jc_init_hw();
}

extern hekate_config h_cfg;

Input_t *hidRead(){
    jc_gamepad_rpt_t *controller = joycon_poll();

    inputs.buttons = 0;
    u8 left_connected = 0;
    u8 right_connected = 0;

    if (controller != NULL){
        if (controller->home)
            RebootToPayloadOrRcm();

        if (controller->cap)
            TakeScreenshot();

        inputs.buttons = controller->buttons;

        left_connected = controller->conn_l;
        right_connected = controller->conn_r;
    }


    u8 btn = btn_read();
    inputs.volp = (btn & BTN_VOL_UP) ? 1 : 0;
    inputs.volm = (btn & BTN_VOL_DOWN) ? 1 : 0;
    inputs.power = (btn & BTN_POWER) ? 1 : 0;

    if (left_connected){
        if ((LbaseX == 0 || LbaseY == 0) || controller->l3){
            LbaseX = controller->lstick_x;
            LbaseY = controller->lstick_y;
        }

        inputs.up = (controller->up || (controller->lstick_y > LbaseY + 500)) ? 1 : 0;
        inputs.down = (controller->down || (controller->lstick_y < LbaseY - 500)) ? 1 : 0;
        inputs.left = (controller->left || (controller->lstick_x < LbaseX - 500)) ? 1 : 0;
        inputs.right = (controller->right || (controller->lstick_x > LbaseX + 500)) ? 1 : 0;
    }
    else {
        inputs.up = inputs.volp;
        inputs.down = inputs.volm;
    }

    if (right_connected){
        if ((RbaseX == 0 || RbaseY == 0) || controller->r3){
            RbaseX = controller->rstick_x;
            RbaseY = controller->rstick_y;
        }

        inputs.rUp = (controller->rstick_y > RbaseY + 500) ? 1 : 0;
        inputs.rDown = (controller->rstick_y < RbaseY - 500) ? 1 : 0;
        inputs.rLeft = (controller->rstick_x < RbaseX - 500) ? 1 : 0;
        inputs.rRight = (controller->rstick_x > RbaseX + 500) ? 1 : 0;
    }
    else
        inputs.a = inputs.power;

    return &inputs;
}

Input_t *hidWaitMask(u32 mask){
    Input_t *in = hidRead();

    while (in->buttons & mask)
        hidRead();

    while (!(in->buttons & mask)){
        hidRead();
    }

    return in;
}

Input_t *hidWait(){
    Input_t *in = hidRead();

    while (in->buttons)
        hidRead();

    while (!(in->buttons))
        hidRead();
    return in;
}

bool hidConnected(){
    jc_gamepad_rpt_t *controller = joycon_poll();
    return (controller->conn_l && controller->conn_r) ? 1 : 0;
}