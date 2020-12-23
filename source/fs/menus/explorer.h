#pragma once
#include "../../utils/vector.h"

typedef struct {
    char *curPath;
    Vector_t *files;
} ExplorerCtx_t;

void FileExplorer(char *path);