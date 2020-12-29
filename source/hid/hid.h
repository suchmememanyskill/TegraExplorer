#pragma once
#include <utils/types.h>

//#define BIT(n) (1U << n)

#define JoyY BIT(0)
#define JoyX BIT(1)
#define JoyB BIT(2)
#define JoyA BIT(3)
#define JoyRB BIT(6)
#define JoyMenu BIT(12)
#define JoyLDown BIT(16)
#define JoyLUp BIT(17)
#define JoyLRight BIT(18)
#define JoyLLeft BIT(19)
#define JoyLB BIT(22)
#define BtnVolP BIT(25)
#define BtnVolM BIT(26)
#define JoyRDown BIT(27)
#define JoyRUp BIT(28)
#define JoyRRight BIT(29)
#define JoyRLeft BIT(30)

#define WAITBUTTONS (JoyY | JoyX | JoyB | JoyA | JoyLDown | JoyLUp | JoyLRight | JoyLLeft)

typedef struct _inputs {
    union {
        struct {
			// Joy-Con (R).
			u32 y:1;
			u32 x:1;
			u32 b:1;
			u32 a:1;
			u32 sr_r:1;
			u32 sl_r:1;
			u32 r:1;
			u32 zr:1;

			// Shared
			u32 minus:1;
			u32 plus:1;
			u32 r3:1;
			u32 l3:1;
			u32 home:1;
			u32 cap:1;
			u32 pad:1;
			u32 wired:1;

			// Joy-Con (L).
			u32 down:1;
			u32 up:1;
			u32 right:1;
			u32 left:1;
			u32 sr_l:1;
			u32 sl_l:1;
			u32 l:1;
			u32 zl:1;

			u32 power:1;
			u32 volp:1;
			u32 volm:1;

			u32 rDown:1;
			u32 rUp:1;
			u32 rRight:1;
			u32 rLeft:1;
		};
		u32 buttons;
    };
} Input_t;

void hidInit();
Input_t *hidRead();
Input_t *hidWait();
Input_t *hidWaitMask(u32 mask);
bool hidConnected();