#pragma once
#include "../common/types.h"

int filemenu(menu_entry file);
void copyfile(const char *src_in, const char *outfolder);
int foldermenu();
void copyfolder(char *in, char *out);