#include "filemenu.h"
#include "../../err.h"
#include "../../gfx/menu.h"

MenuEntry_t FileMenuEntries[] = {
    // Still have to think up the options
};

void FileMenu(FSEntry_t entry){
    DrawError(newErrCode(TE_ERR_UNIMPLEMENTED));
} 