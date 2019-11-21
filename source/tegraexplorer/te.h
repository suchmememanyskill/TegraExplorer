#pragma once
#include "../utils/types.h"

#define MAINMENU_AMOUNT 6
#define CREDITS_MESSAGE "Tegraexplorer, made by:\nSuch Meme, Many Skill\n\nProject based on:\nLockpick_RCM\nHekate\n\nCool people:\nshchmue\ndennthecafebabe\nDax"

typedef struct _menu_item {
    char name[50];
    u32 color;
    short internal_function;
    short property;
} menu_item;

menu_item mainmenu[MAINMENU_AMOUNT];

void te_main();