#pragma once
#include "../utils/types.h"

#define BIT(n) (1 << n)
#define KEY_A BIT(3)
#define KEY_B BIT(2)
#define KEY_LUP BIT(18)
#define KEY_LDOWN BIT(17)
#define KEY_RUP BIT(7)
#define KEY_RDOWN BIT(6)
#define KEY_VOLP BIT(14)
#define KEY_VOLM BIT(15)
#define KEY_POW BIT(16)

typedef struct _inputs {
    union {
        struct {
            // Joy-Con (R).
			u32 y:1;
			u32 x:1;
			u32 b:1;
			u32 a:1;
			u32 r:1;
			u32 zr:1;
            u32 Rdown:1;
            u32 Rup:1;
            u32 Rright:1;
            u32 Rleft:1;

			// Shared
			u32 minus:1;
			u32 plus:1;
			u32 home:1;
			u32 cap:1;
            u32 volp:1;
            u32 volm:1;
            u32 pow:1;

			// Joy-Con (L).
			u32 Ldown:1;
			u32 Lup:1;
			u32 Lright:1;
			u32 Lleft:1;
			u32 l:1;
			u32 zl:1;
        };
        u32 buttons;
    };
} Inputs;

void hidInit();
Inputs *hidRead();
Inputs *hidWait();
Inputs *hidWaitMask(u32 mask);