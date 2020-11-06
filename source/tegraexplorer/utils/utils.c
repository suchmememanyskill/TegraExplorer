#include <string.h>
#include "utils.h"
#include "../common/common.h"
#include "../gfx/menu.h"
#include "../../storage/emummc.h"
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../../hid/hid.h"
#include "../../utils/util.h"
#include "../../utils/sprintf.h"
#include "../../libs/fatfs/ff.h"
#include "../fs/fsutils.h"
#include "../../mem/minerva.h"
#include "../../storage/nx_sd.h"
#include "../../gfx/di.h"

extern bool sd_mounted;

int utils_mmcMenu(){
    if (emu_cfg.enabled)
        return menu_make(utils_mmcChoice, 3, "-- Choose MMC --");    
    else
        return SYSMMC;
}

void utils_copystring(const char *in, char **out){
    int len = strlen(in) + 1;
    *out = (char *) malloc (len);
    strcpy(*out, in);
}


void utils_takeScreenshot(){
    static u32 timer = 0;

    if (!minerva_cfg || !sd_mounted)
		return;

    if (timer + 3 < get_tmr_s())
        timer = get_tmr_s();
    else 
        return;

    char *name, *path;
    char basepath[] = "sd:/tegraexplorer/screenshots";
    name = malloc(40);
    sprintf(name, "Screenshot_%08X.bmp", get_tmr_us());

    f_mkdir("sd:/tegraexplorer");
    f_mkdir(basepath);
    path = fsutil_getnextloc(basepath, name);
    free(name);

    const u32 file_size = 0x384000 + 0x36;
    u8 *bitmap = malloc(file_size);
    u32 *fb = malloc(0x384000);
    u32 *fb_ptr = gfx_ctxt.fb;

    for (int x = 1279; x >= 0; x--)
	{
		for (int y = 719; y >= 0; y--)
			fb[y * 1280 + x] = *fb_ptr++;
	}

    memcpy(bitmap + 0x36, fb, 0x384000);
    bmp_t *bmp = (bmp_t *)bitmap;

	bmp->magic    = 0x4D42;
	bmp->size     = file_size;
	bmp->rsvd     = 0;
	bmp->data_off = 0x36;
	bmp->hdr_size = 40;
	bmp->width    = 1280;
	bmp->height   = 720;
	bmp->planes   = 1;
	bmp->pxl_bits = 32;
	bmp->comp     = 0;
	bmp->img_size = 0x384000;
	bmp->res_h    = 2834;
	bmp->res_v    = 2834;
	bmp->rsvd2    = 0;

    sd_save_to_file(bitmap, file_size, path);
    free(bitmap);
    free(fb);

    display_backlight_brightness(255, 1000);
	msleep(100);
	display_backlight_brightness(100, 1000);
}


char *utils_InputText(char *start, int maxLen){
    if (!hidConnected())
        return NULL;

    int offset = -1, currentPos = 0, len;
    char temp;
    Inputs *input = hidRead();
    u32 x, y;
    gfx_printf("Add characters by pressing X\nRemove characters by pressing Y\nJoysticks for movement\nB to cancel, A to accept\n\n");
    gfx_con_getpos(&x, &y);

    if (strlen(start) > maxLen)
        return NULL;

    char *buff;
    buff = calloc(maxLen + 1, sizeof(char));
    strcpy(buff, start);

    while (1){
        offset = -1;
        gfx_con_setpos(x, y);
        for (int i = 0; i < 3; i++){
            if (offset != 0){
                SWAPCOLOR(0xFF666666);
                gfx_con.fntsz = 8;
            }
            else {
                SWAPCOLOR(COLOR_WHITE);
                gfx_con.fntsz = 16;
            }
            for (int x = 0; x < strlen(buff); x++){
                if (offset == 0 && x == currentPos){
                    gfx_printf("%k%c%k", COLOR_GREEN, buff[x], COLOR_WHITE);
                }
                else {
                    temp = buff[x] + offset;

                    if (!(temp >= 32 && temp <= 126))
                        temp = ' ';

                    gfx_putc(temp);
                }

                if (offset != 0)
                    gfx_puts("   ");
                else
                    gfx_putc(' ');
            }
            gfx_putc('\n');
            offset++;
        }

        if (input->buttons & (KEY_RDOWN | KEY_RUP))
            hidRead();
        else
            hidWait();

        len = strlen(buff);

        if (input->buttons & (KEY_A | KEY_B))
            break;

        if (input->buttons & (KEY_LDOWN | KEY_RDOWN) && buff[currentPos] < 126){
            temp = ++buff[currentPos];
            while (strchr("\\\"*/:<=>?|+;=[]", temp) != NULL)
                temp = ++buff[currentPos];
        }
        
        if (input->buttons & (KEY_LUP | KEY_RUP) && buff[currentPos] > 32){
            temp = --buff[currentPos];
            while (strchr("\\\"*/:<=>?|+;=[]", temp) != NULL)
                temp = --buff[currentPos];
        }

        if (input->Lleft && currentPos > 0)
            currentPos--;

        if (input->Lright && currentPos < len - 1)
            currentPos++;
            
        if (input->x && maxLen > len){
            buff[len] = '.';
            buff[len + 1] = '\0';
        }

        if (input->y && len > 1){
            buff[len - 1] = '\0';
            if (currentPos == len - 1){
                currentPos--;
            }
            gfx_boxGrey(0, y, 1279, y + 48, 0x1B);
        }
    }

    gfx_con.fntsz = 16;

    if (input->b){
        free(buff);
        return NULL;
    }

    return buff;
}

char *utils_copyStringSize(const char *in, int size){
    if (size > strlen(in) || size < 0)
        size = strlen(in);

    char *out = calloc(size + 1, 1);
    //strncpy(out, in, size);
    memcpy(out, in, size);
    return out;
}

char* util_cpyStr(const char* in){
    char *out = malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}