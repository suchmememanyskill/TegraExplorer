#include "tconf.h"
#include <mem/heap.h>
#include "../utils/utils.h"
TConf_t TConf = {0};

void ResetCopyParams(){
    TConf.heldExplorerCopyLoc = LOC_None;
    FREE(TConf.srcCopy);
    TConf.explorerCopyMode = CMODE_None;
}

void SetCopyParams(const char *path, u8 mode){
    ResetCopyParams();
    TConf.heldExplorerCopyLoc = TConf.curExplorerLoc;
    TConf.explorerCopyMode = mode;
    TConf.srcCopy = CpyStr(path);
}