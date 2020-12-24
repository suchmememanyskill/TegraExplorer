#include "err.h"
#include "gfx/gfx.h"
#include "hid/hid.h"
#include "gfx/gfxutils.h"

const char *fatfsErrors[] = {
    "I/O ERROR",
    "NO DISK",
    "NOT READY",
    "NO FILE",
    "NO PATH",
    "PATH INVALID",
    "ACCESS DENIED",
    "ACCESS DENIED",
    "INVALID PTR",
    "PROTECTED",
    "INVALID DRIVE",
    "NO MEM",
    "NO FAT",
    "MKFS ABORT"
};

const char *TEErrors[] = {
    "Unimplemented"
};

const char *GetErrStr(u32 err){
    --err; // obv error codes cannot be 0
    if (err >= 0 && err < ARRAY_SIZE(fatfsErrors))
        return fatfsErrors[err];

    if (err >= 20 && err < ARRAY_SIZE(TEErrors) + 20)
        return TEErrors[err - 20];

    return "(Unknown)";
}

#define lx 256
#define ly 240
#define lenx 768
#define leny 240

void DrawError(ErrCode_t err){
    SETCOLOR(COLOR_ORANGE, COLOR_DARKGREY);
    gfx_box(lx, ly, lx + lenx, ly + leny, COLOR_ORANGE);
    gfx_boxGrey(lx + 16, ly + 16, lx + lenx - 16, ly + leny - 16, 0x33);
    gfx_con_setpos(lx + ((lenx - 17 * 16) / 2), ly + 32);
    gfx_printf("An error occured!\n\n%bErr : %d\nLine: %d\nFile: %s\nDesc: %s%b", lx + 48, err.err, err.loc, err.file, GetErrStr(err.err), 0);
    gfx_con_setpos(lx + ((lenx - 19 * 16) / 2), ly + leny - 48);
    gfx_printf("Press A to continue");
    
    hidWaitMask((JoyA | JoyB));
}