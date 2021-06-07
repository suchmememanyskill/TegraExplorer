#include "tools.h"
#include "../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../gfx/menu.h"
#include "../hid/hid.h"
#include <libs/fatfs/ff.h>
#include "../keys/keys.h"
#include "../keys/nca.h"
#include <storage/nx_sd.h>
#include "../fs/fsutils.h"
#include <utils/util.h>
#include "../storage/mountmanager.h"
#include "../err.h"
#include <utils/sprintf.h>
#include <mem/heap.h>
#include "../tegraexplorer/tconf.h"
#include "../fs/readers/folderReader.h"
#include <string.h>
#include "../fs/fscopy.h"
#include "../utils/utils.h"

void DumpSysFw(){
	char sysPath[25 + 36 + 3 + 1]; // 25 for "bis:/Contents/registered", 36 for ncaName.nca, 3 for /00, and 1 to make sure :)
	char *baseSdPath;

	u32 timer = get_tmr_s();

	if (!sd_mount())
		return;

	if (connectMMC(MMC_CONN_EMMC))
		return;

	if (!TConf.keysDumped)
		return;

	ErrCode_t err = mountMMCPart("SYSTEM");
	if (err.err){
		DrawError(err);
		return;
	}

	baseSdPath = malloc(36 + 16);
	sprintf(baseSdPath, "sd:/tegraexplorer/Firmware/%d (%s)", TConf.pkg1ver, TConf.pkg1ID);
	int baseSdPathLen = strlen(baseSdPath);

	f_mkdir("sd:/tegraexplorer");
	f_mkdir("sd:/tegraexplorer/Firmware");

	gfx_clearscreen();

	gfx_printf("Pkg1 id: '%s', kb %d\n", TConf.pkg1ID, TConf.pkg1ver);
	if (FileExists(baseSdPath)){
		SETCOLOR(COLOR_ORANGE, COLOR_DEFAULT);
		gfx_printf("Destination already exists. Replace?   ");
		if (!MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
			free(baseSdPath);
			return;
		}
		RESETCOLOR;
		gfx_printf("\nDeleting... ");
		FolderDelete(baseSdPath);
		gfx_putc('\n');
	}

	f_mkdir(baseSdPath);

	gfx_printf("Out: %s\nReading entries...\n", baseSdPath);
	int readRes = 0;
	Vector_t fileVec = ReadFolder("bis:/Contents/registered", &readRes);
	if (readRes){
		DrawError(newErrCode(readRes));
		free(baseSdPath);
		return;
	}

	gfx_printf("Starting dump...\n");
	SETCOLOR(COLOR_GREEN, COLOR_DEFAULT);

	int res = 0;
	int total = 1;
	vecDefArray(FSEntry_t*, fsEntries, fileVec);
	for (int i = 0; i < fileVec.count; i++){
		sprintf(sysPath, (fsEntries[i].isDir) ? "%s/%s/00" : "%s/%s", "bis:/Contents/registered", fsEntries[i].name);
		int contentType = GetNcaType(sysPath);

		if (contentType < 0){
			res = 1;
			break;
		}

		char *sdPath = malloc(baseSdPathLen + 45);
		sprintf(sdPath, "%s/%s", baseSdPath, fsEntries[i].name);
		if (contentType == Meta)
			memcpy(sdPath + strlen(sdPath) - 4, ".cnmt.nca", 10);
		
		gfx_printf("[%3d / %3d] %s\r", total, fileVec.count, fsEntries[i].name);
		total++;
		err = FileCopy(sysPath, sdPath, 0);
		free(sdPath);
		if (err.err){
			DrawError(err);
			res = 1;
			break;
		}
	}
	clearFileVector(&fileVec);

	RESETCOLOR;

	if (res){
		gfx_printf("\nDump failed...\n");
	}

	gfx_printf("\n\nDone! Time taken: %ds\nPress any key to exit", get_tmr_s() - timer);
	free(baseSdPath);
	hidWait();
}

extern sdmmc_storage_t sd_storage;
extern bool is_sd_inited;

MenuEntry_t FatAndEmu[] = {
	{.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Back to main menu"},
	{.optionUnion = COLORTORGB(COLOR_GREEN), .name = "Fat32 + EmuMMC"},
	{.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Only Fat32"}
};

void FormatSD(){
	gfx_clearscreen();
	disconnectMMC();
	DWORD plist[] = {0,0,0,0};
	bool emummc = 0;
	int res;

	if (!is_sd_inited || sd_get_card_removed())
		return;

	gfx_printf("\nDo you want to partition for an emummc?\n");
	res = MakeHorizontalMenu(FatAndEmu, ARR_LEN(FatAndEmu), 3, COLOR_DEFAULT, 0);
	
	if (!res)
		return;

	emummc = !(res - 1);
	
	SETCOLOR(COLOR_RED, COLOR_DEFAULT);

	plist[0] = sd_storage.csd.capacity;
	if (emummc){
		if (plist[0] < 83886080){
            gfx_printf("\n\nYou seem to be running this on a 32GB or smaller SD\nNot enough free space for emummc!");
			hidWait();
			return;
        }
		plist[0] -= 61145088;
		u32 allignedSectors = plist[0] - plist[0] % 2048;
		plist[1] = 61145088 + plist[0] % 2048;
		plist[0] = allignedSectors;
	}

	gfx_printf("\n\nAre you sure you want to format your sd?\nThis will delete everything on your SD card!\nThis action is irreversible!\n\n");
	WaitFor(1500);

	gfx_printf("%kAre you sure?   ", COLOR_WHITE);
	if (!MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
		return;
	}

	RESETCOLOR;

	gfx_printf("\n\nStarting Partitioning & Formatting\n");

	for (int i = 0; i < 2; i++){
		gfx_printf("Part %d: %dKiB\n", i + 1, plist[i] / 2);
	}

	u8 *work = malloc(TConf.FSBuffSize);
	res = f_fdisk_mod(0, plist, work);

	if (!res){
		res = f_mkfs("sd:", FM_FAT32, 32768, work, TConf.FSBuffSize);
	}

	sd_unmount();

	if (res){
		DrawError(newErrCode(res));
		gfx_clearscreen();
		gfx_printf("Something went wrong\nPress any key to exit");
	}
	else {
		sd_mount();
		gfx_printf("\nDone!\nPress any key to exit");
	}

	free(work);
	hidWait();
}

extern bool sd_mounted;

void TakeScreenshot(){
    static u32 timer = 0;

    if (!TConf.minervaEnabled || !sd_mounted)
		return;

    if (timer + 3 < get_tmr_s())
        timer = get_tmr_s();
    else 
        return;

    char *name, *path;
    const char basepath[] = "sd:/tegraexplorer/screenshots";
    name = malloc(40);
    sprintf(name, "Screenshot_%08X.bmp", get_tmr_us());

    f_mkdir("sd:/tegraexplorer");
    f_mkdir(basepath);
    path = CombinePaths(basepath, name);
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
	free(path);

    display_backlight_brightness(255, 1000);
	msleep(100);
	display_backlight_brightness(100, 1000);
}
