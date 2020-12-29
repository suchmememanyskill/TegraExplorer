#pragma once
#include <utils/types.h>
#include "fstypes.h"

FSEntry_t GetFileInfo(const char *path);
char *EscapeFolder(const char *current);
char *CombinePaths(const char *current, const char *add);
char *GetFileAttribs(FSEntry_t entry);
bool FileExists(char* path);