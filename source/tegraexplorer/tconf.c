#include "tconf.h"
#include <mem/heap.h>
TConf_t TConf = {0};

void ResetCopyParams(){
    TConf.heldExplorerCopyLoc = LOC_None;
    if (TConf.srcCopy != NULL)
        free(TConf.srcCopy);
    TConf.explorerCopyMode = CMODE_None;
}

void SetCopyParams(char *path, u8 mode){
    ResetCopyParams();
    TConf.heldExplorerCopyLoc = TConf.curExplorerLoc;
    TConf.explorerCopyMode = mode;
    TConf.srcCopy = path;
}