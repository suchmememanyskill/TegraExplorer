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

int newMenu(Vector_t* vec, int startIndex, int screenLenX, int screenLenY, menuEntriesGatherer gatherer, void *ctx, bool enableB, int totalEntries);
void FunctionMenuHandler(MenuEntry_t *entries, int entryCount, menuPaths *paths, bool enableB);