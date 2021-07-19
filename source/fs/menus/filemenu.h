#pragma once
#include "../fstypes.h"

typedef void (*fileMenuPath)(char *path, FSEntry_t entry);

void FileMenu(char *path, FSEntry_t entry);
void RunScript(char *path, FSEntry_t entry);
void RunScriptString(char *str, u32 size);