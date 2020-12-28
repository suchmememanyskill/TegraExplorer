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

void TestControllers(){
    gfx_clearscreen();
    gfx_printf("Controller test screen. Return using b\n\n");
    while (1){
		Input_t *controller = hidRead();

        if (controller->b)
            return;

		u32 buttons = controller->buttons;
		for (int i = 0; i < 31; i++){
			gfx_printf("%d", buttons & 1);
			buttons >>= 1;
		}
		gfx_printf("\r");
	}
}

extern int launch_payload(char *path);

void RebootToPayload(){
    launch_payload("atmosphere/reboot_payload.bin");
}

void DumpSysFw(){
	char sysPath[25 + 36 + 3 + 1]; // 24 for "bis:/Contents/registered", 36 for ncaName.nca, 3 for /00, and 1 to make sure :)
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
		gfx_printf("\nReminder! delete the folder. i can't delete recursively yet");
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
	res = MakeHorizontalMenu(FatAndEmu, ARR_LEN(FatAndEmu), 3, COLOR_DEFAULT);
	
	if (!res)
		return;

	emummc = !(res - 1);
	
	plist[0] = sd_storage.csd.capacity;
	if (emummc){
		plist[0] -= 61145088;
		u32 allignedSectors = plist[0] - plist[0] % 2048;
		plist[1] = 61145088 + plist[0] % 2048;
		plist[0] = allignedSectors;
	}

	SETCOLOR(COLOR_RED, COLOR_DEFAULT);
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