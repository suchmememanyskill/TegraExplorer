#pragma once
#include "../../utils/vector.h"
#include "../../gfx/menu.h"
#include "../fstypes.h"

void FileExplorer(char *path);
MenuEntry_t MakeMenuOutFSEntry(FSEntry_t entry);