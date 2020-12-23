#include "tools.h"
#include "../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../gfx/menu.h"
#include "../hid/hid.h"

void TestControllers(){
    gfx_clearscreen();
    gfx_printf("Controller test screen. Return using b\n\n");
    while (1){
		Input_t *controller = hidRead();

        if (controller->b)
            return;

		u32 buttons = controller->buttons;
		for (int i = 0; i < 31; i++){
			gfx_printf("%d", buttons & 1);
			buttons >>= 1;
		}
		gfx_printf("\r");
	}
}

extern int launch_payload(char *path);

void RebootToPayload(){
    launch_payload("atmosphere/reboot_payload.bin");
}