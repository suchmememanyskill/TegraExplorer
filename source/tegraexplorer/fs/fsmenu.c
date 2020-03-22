#include <string.h>
#include "fsmenu.h"
#include "fsreader.h"
#include "../gfx/menu.h"
#include "../gfx/gfxutils.h"
#include "fsutils.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "entrymenu.h"

extern char *currentpath;
extern char *clipboard;
extern u8 clipboardhelper;
int lastentry = 0;

void fileexplorer(const char *startpath, int type){
    int res;
    
    if (lastentry > 0 && lastentry == type)
        clipboardhelper = 0;
    
    lastentry = type;
    
    fsreader_writecurpath(startpath);
    fsreader_readfolder(currentpath);

    /*
    if (strcmp(rootpath, "emmc:/"))
        explfilemenu[6].property = 1;
    else
        explfilemenu[6].property = -1;
    */

    while (1){
        res = menu_make(fsreader_files, fsutil_getfileobjamount(fsreader_files), currentpath);
        switch (res){
            case FILEMENU_RETURN:
                if (!strcmp(startpath, currentpath))
                    return;
                else {
                    fsreader_writecurpath(fsutil_getprevloc(currentpath));
                    fsreader_readfolder(currentpath);
                }

                break;
            case FILEMENU_CLIPBOARD:
                if (clipboardhelper & ISDIR)
                    copyfolder(clipboard, currentpath);
                else
                    copyfile(clipboard, currentpath);
                break;
            
            case FILEMENU_CURFOLDER:
                if (foldermenu())
                    return;
                break;
            case -1:
                return;

            default:
                if(fsreader_files[res].property & ISDIR){
                    fsreader_writecurpath(fsutil_getnextloc(currentpath, fsreader_files[res].name));
                    fsreader_readfolder(currentpath);
                }
                else
                    if(filemenu(fsreader_files[res]))
                        return;
                
                break;
        }
    }
    /*
    while (1){
        res = makefilemenu(fileobjects, getfileobjamount(), currentpath);
        if (res < 1){
            switch (res){
                case -2:
                    if (!strcmp(rootpath, currentpath))
                        breakfree = true;
                    else {
                        writecurpath(getprevloc(currentpath));
                        readfolder(currentpath);
                    }

                    break;
                
                case -1:
                    if (clipboardhelper & ISDIR)
                        copyfolder(clipboard, currentpath);
                    else
                        copyfile(clipboard, currentpath);
                    break;

                case 0:
                    tempint = foldermenu();

                    if (tempint == -1)
                        breakfree = true;

                    break;
                
            }
        }
        else {
            if (fileobjects[res - 1].property & ISDIR){
                writecurpath(getnextloc(currentpath, fileobjects[res - 1].name));
                readfolder(currentpath);
            }
            else {
                filemenu(fileobjects[res - 1]);
            }
        }

        if (breakfree)
            break;
    }
    */
}