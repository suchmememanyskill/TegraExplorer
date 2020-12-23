#pragma once
#include <utils/types.h>

u64 GetFileSize(char *path);
char *EscapeFolder(char *current);
char *CombinePaths(const char *current, const char *add);