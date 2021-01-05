#pragma once
#include <utils/types.h>
#include "../utils/vector.h"

typedef void (*menuEntriesGatherer)(Vector_t* vec, void* data);
typedef void (*menuPaths)();

typedef struct _menuEntry {
    union {
        struct {
            u32 B:8;
            u32 G:8;
            u32 R:8;
            u32 skip:1;
            u32 hide:1;
        };
        u32 optionUnion;
    };
    char *name;
    u8 icon;
    union {
        struct {
            u16 size:12;
            u16 showSize:1;
            u16 sizeDef:3;
        };
        u16 sizeUnion;
    };
} MenuEntry_t;

#define SKIPHIDEBITS 0x3000000
#define RGBCOLOR(r, g, b) (b << 16 | g << 8 | r)

#define SKIPBIT BIT(24)
#define HIDEBIT BIT(25)

#define ENABLEB BIT(0)
#define ENABLEPAGECOUNT BIT(1)
#define ALWAYSREDRAW BIT(2)
#define USELIGHTGREY BIT(3)

#define ScreenDefaultLenX 79
#define ScreenDefaultLenY 30

#define ARR_LEN(x) (sizeof(x) / sizeof(*x))

int newMenu(Vector_t* vec, int startIndex, int screenLenX, int screenLenY, u8 options, int entryCount);
void FunctionMenuHandler(MenuEntry_t *entries, int entryCount, menuPaths *paths, u8 options);