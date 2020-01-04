#pragma once
#include "te.h"
#include "fs.h"

#define SWAPCOLOR(color) gfx_printf("%k", color)
#define RESETCOLOR gfx_printf("%k%K", COLOR_WHITE, COLOR_DEFAULT)

int makemenu(menu_item menu[], int menuamount);
int message(u32 color, const char* message, ...);
void clearscreen();
int makefilemenu(fs_entry *files, int amount, char *path);
void printbytes(u8 print[], u32 size, u32 offset);
int makewaitmenu(char *initialmessage, char *hiddenmessage, int timer);
void gfx_print_length(int size, char *toprint);
