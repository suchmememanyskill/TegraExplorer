#pragma once
#include "../common/types.h"

extern menu_entry *fsreader_files;
void fsreader_writecurpath(const char *in);
void fsreader_writeclipboard(const char *in, u8 args);
int fsreader_readfolder(const char *path);