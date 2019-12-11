#pragma once
#include "te.h"
#include "fs.h"

int makemenu(menu_item menu[], int menuamount);
int message(char* message, u32 color);
void clearscreen();
int makefilemenu(fs_entry *files, int amount, char *path);
void printbytes(u8 print[], u32 size, u32 offset);
int makewaitmenu(char *initialmessage, char *hiddenmessage, int timer);