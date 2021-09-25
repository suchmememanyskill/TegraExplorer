#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "eval.h"
#include "garbageCollector.h"
#include "intClass.h"
#include "standardLibrary.h"
#include "scriptError.h"
#include <string.h>
#include "dictionaryClass.h"
#include "StringClass.h"

#ifndef WIN32
#include "../storage/mountmanager.h"
#include "../keys/keys.h"
#include "../fs/readers/folderReader.h"
#include "../fs/fscopy.h"
#include <mem/heap.h>
#include "../keys/nca.h"
#include "../hid/hid.h"
#include "../gfx/menu.h"
#include "../gfx/gfxutils.h"
#include "../tegraexplorer/tconf.h"
#include "../storage/emummc.h"
#include <utils/util.h>
#include "../fs/fsutils.h"
#include <storage/nx_sd.h>
#include "../storage/emmcfile.h"
#include <soc/fuse.h>
#endif
// Takes [int, function]. Returns elseable.
ClassFunction(stdIf) {
	s64 value = getIntValue(args[0]);

	if (value) {
		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL)
			return NULL;

		removePendingReference(res);
	}

	Variable_t* ret = newIntVariablePtr(value);
	ret->variableType = ElseClass;

	return ret;
}

// Takes [function, function]. Returns empty. Works by evaling the first function and running the 2nd if true.
ClassFunction(stdWhile) {
	Variable_t* result = eval(args[0]->function.function.operations.data, args[0]->function.function.operations.count, 1);
	if (result == NULL || result->variableType != IntClass)
		return NULL;

	while (result->integer.value) {
		removePendingReference(result);
		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL) {
			if (scriptLastError == SCRIPT_BREAK) {
				break;
			}
			else {
				return NULL;
			}
		}

		removePendingReference(res);

		result = eval(args[0]->function.function.operations.data, args[0]->function.function.operations.count, 1);
		if (result == NULL || result->variableType != IntClass)
			return NULL;
	}

	removePendingReference(result);

	return &emptyClass;
}

// Takes [???]. Returns empty. Works by calling .print on every argument
ClassFunction(stdPrint) {
	for (int i = 0; i < argsLen; i++) {
		Variable_t* res = callMemberFunctionDirect(args[i], "print", NULL, 0);
		if (res == NULL)
			return NULL;
		if (i + 1 != argsLen)
			gfx_putc(' ');
	}
	

	return &emptyClass;
}

// Takes [???]. Returns empty. Calls stdPrint
ClassFunction(stdPrintLn) {
	stdPrint(caller, args, argsLen);
	gfx_printf("\n");
	return &emptyClass;
}

// Takes none. Returns none. Returning NULL will cause a cascade of errors and will exit runtime
ClassFunction(stdExit) {
	return NULL;
}

// Takes none. Returns none. See stdExit. stdWhile and array.foreach look for SCRIPT_BREAK and break when seen.
ClassFunction(stdBreak) {
	scriptLastError = SCRIPT_BREAK;
	return NULL;
}

// Takes none. Returns empty dictionary.
ClassFunction(stdDict) {
	Variable_t a = { 0 };
	a.variableType = DictionaryClass;
	a.dictionary.vector = newVec(sizeof(Dict_t), 0);
	return copyVariableToPtr(a);
}

#ifndef WIN32


int mountMmc(u8 mmc, char *part){
	if (connectMMC(mmc))
		return 1;

	if (mountMMCPart(part).err)
		return 1;

	return 0;
}

// Takes [str]. Returns int (0=success). str=partition to mount
ClassFunction(stdMountSysmmc){
	return newIntVariablePtr(mountMmc(MMC_CONN_EMMC, args[0]->string.value));
}

ClassFunction(stdMountEmummc){
	if (!emu_cfg.enabled){
		SCRIPT_FATAL_ERR("emummc is not enabled");
	}

	return newIntVariablePtr(mountMmc(MMC_CONN_EMUMMC, args[0]->string.value));
}

// Takes [str]. Returns int (0=success) str=path to save
ClassFunction(stdMountSave){
	
	Variable_t *arg = (*args);
	Variable_t var = {.variableType = SaveClass};
	SaveClass_t* save = calloc(1, sizeof(SaveClass_t));
	if (f_open(&save->saveFile, arg->string.value, FA_READ | FA_WRITE))
		return NULL;
	save_init(&save->saveCtx, &save->saveFile, dumpedKeys.save_mac_key, 0);
	if (!save_process(&save->saveCtx))
		return NULL;
	
	var.save = save;
	return copyVariableToPtr(var);
}

// Takes [int, int, int]. Returns empty. 0: posX, 1: posY, 2: hexColor
ClassFunction(stdSetPixel) {
	u32 color = getIntValue(args[2]);
	gfx_set_pixel_horz(args[0]->integer.value, args[1]->integer.value, color);
	return &emptyClass;
}

ClassFunction(stdSetPixels){
	gfx_box(getIntValue(args[0]), getIntValue(args[1]), getIntValue(args[2]), getIntValue(args[3]), (u32)getIntValue(args[4]));
	return &emptyClass;
}

ClassFunction(stdSetPrintPos){
	if (getIntValue(args[0]) > 0 && getIntValue(args[1]) > 0){
		gfx_con_setpos((getIntValue(args[0]) % 78) * 16, (getIntValue(args[1]) % 42) * 16);
	}
	return &emptyClass;
}

// Takes [str]. Returns empty. str: path to dir
ClassFunction(stdReadDir){
	Variable_t* resPtr = newIntVariablePtr(0);
	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = newVec(sizeof(Dict_t), 4)};
	addVariableToDict(&ret, "result", resPtr);

	Variable_t fileNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t dirNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t fileSizeArray = {.variableType = IntArrayClass, .solvedArray.vector = newVec(sizeof(s64), 0)};

	DIR dir;
	if ((resPtr->integer.value = f_opendir(&dir, args[0]->string.value))){
		return copyVariableToPtr(ret);
	}

	FILINFO fno;
	while (!(resPtr->integer.value = f_readdir(&dir, &fno)) && fno.fname[0]){
		char *name = CpyStr(fno.fname);
		if (fno.fattrib & AM_DIR){
			vecAdd(&dirNamesArray.solvedArray.vector, name);
		}
		else {
			vecAdd(&fileNamesArray.solvedArray.vector, name);
			s64 size = fno.fsize;
			vecAdd(&fileSizeArray.solvedArray.vector, size);
		}
	}

	f_closedir(&dir);

	addVariableToDict(&ret, "files", copyVariableToPtr(fileNamesArray));
	addVariableToDict(&ret, "folders", copyVariableToPtr(dirNamesArray));
	addVariableToDict(&ret, "fileSizes", copyVariableToPtr(fileSizeArray));
	
	return copyVariableToPtr(ret);
}

char *abxyNames[] = {
	"y",
	"x",
	"b",
	"a"
};

char *ulrdNames[] = {
	"down",
	"up",
	"right",
	"left",
};

char *powNames[] = {
	"power",
	"volplus",
	"volminus",
};

Variable_t *hidToVar(u32 raw){
	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = newVec(sizeof(Dict_t), 9)};
	addIntToDict(&ret, "raw", raw);

	for (int i = 0; i < ARRAY_SIZE(abxyNames); i++){
		addIntToDict(&ret, abxyNames[i], raw & 0x1);
		raw >>= 1;
	}

	raw >>= 12;

	for (int i = 0; i < ARRAY_SIZE(ulrdNames); i++){
		addIntToDict(&ret, ulrdNames[i], raw & 0x1);
		raw >>= 1;
	}

	raw >>= 4;

	for (int i = 0; i < ARRAY_SIZE(powNames); i++){
		addIntToDict(&ret, powNames[i], raw & 0x1);
		raw >>= 1;
	}

	return copyVariableToPtr(ret);
}

ClassFunction(stdRead){
	Input_t *i = hidRead();
	u32 raw = i->buttons;
	return hidToVar(raw); 
}

// Takes [int]. Returns dict[a,b,x,y,down,up,right,left,power,volplus,volminus,raw]. int: mask for hidWaitMask 
ClassFunction(stdPauseMask){
	Input_t *i = hidWaitMask((u32)getIntValue(*args));
	u32 raw = i->buttons;
	return hidToVar(raw);
}

// Takes none. Returns dict (same as stdPauseMask). 
ClassFunction(stdPause){
	Variable_t a = {.integer.value = 0xFFFFFFFF};
	Variable_t *b = &a;
	return stdPauseMask(caller, &b, 1);
}

// Takes [str, str]. Returns int (0=success). 0: src path, 1: dst path
ClassFunction(stdFileCopy){
	ErrCode_t e = FileCopy(args[0]->string.value, args[1]->string.value, COPY_MODE_PRINT);
	return newIntVariablePtr(e.err);
}

ClassFunction(stdMkdir){
	return newIntVariablePtr(f_mkdir(args[0]->string.value));
}

// Broken????
ClassFunction(stdGetMemUsage){
	heap_monitor_t mon;
	heap_monitor(&mon, false);
	Dict_t a = {.name = CpyStr("used"), .var = newIntVariablePtr((s64)mon.used)};
	Dict_t b = {.name = CpyStr("total"), .var = newIntVariablePtr((s64)mon.total)};
	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = newVec(sizeof(Dict_t), 2)};
	vecAdd(&ret.dictionary.vector, a);
	vecAdd(&ret.dictionary.vector, b);
	return copyVariableToPtr(ret);
}

// Takes [int]. Returns empty. int: hex color
ClassFunction(stdColor){
	gfx_con_setcol((u32)getIntValue(*args) | 0xFF000000, gfx_con.fillbg, gfx_con.bgcol);
	return &emptyClass;
}

// Takes [str]. Returns int. str: path to nca
ClassFunction(stdGetNcaType){
	int type = GetNcaType(args[0]->string.value);
	return newIntVariablePtr(type);
}

// Takes [str[], int, int[]]. Returns int (index). str[]: names of entries, int: starting index, int[]: colors & options. The 3rd argument is optional
ClassFunction(stdMenuFull){
	if (argsLen > 2){
		if (args[2]->solvedArray.vector.count < args[0]->solvedArray.vector.count){
			SCRIPT_FATAL_ERR("invalid menu args");
		}
	}

	Vector_t v = newVec(sizeof(MenuEntry_t), args[0]->solvedArray.vector.count);
	
	vecDefArray(char**, menuEntryNames, args[0]->solvedArray.vector);
	vecDefArray(s64*, menuEntryOptions, args[2]->solvedArray.vector);

	for (int i = 0; i < args[0]->solvedArray.vector.count; i++){
		MenuEntry_t a = {.name = menuEntryNames[i]};
		if (argsLen > 2){
			u32 options = (u32)menuEntryOptions[i];
			if (options & BIT(26)){
				a.icon = 128;
			}
			else if (options & BIT(27)){
				a.icon = 127;
			}

			a.optionUnion = options;
		}
		else {
			a.optionUnion = COLORTORGB(COLOR_WHITE);
		}

		vecAdd(&v, a);
	}

	u32 x=0,y=0;
	gfx_con_getpos(&x,&y);
	int res = newMenu(&v, getIntValue(args[1]), ScreenDefaultLenX - ((x + 1) / 16), 40 - ((y + 1) / 16) - 1, ENABLEB | ALWAYSREDRAW, 0);
	vecFree(v);
	return newIntVariablePtr(res);
}

ClassFunction(stdHasEmu){
	return newIntVariablePtr(emu_cfg.enabled);
}

ClassFunction(stdClear){
	gfx_clearscreen();
	return &emptyClass;
}

ClassFunction(stdRmDir){
	return newIntVariablePtr(FolderDelete(args[0]->string.value).err);
}

ClassFunction(stdGetMs){
	return newIntVariablePtr(get_tmr_ms());
}

ClassFunction(stdFileExists){
	return newIntVariablePtr(FileExists(args[0]->string.value));
}

ClassFunction(stdFileDel){
	return newIntVariablePtr(f_unlink(args[0]->string.value));
}

ClassFunction(stdCopyDir){
	return newIntVariablePtr(FolderCopy(args[0]->string.value, args[1]->string.value).err);
}

ClassFunction(stdFileMove){
	return newIntVariablePtr(f_rename(args[0]->string.value, args[1]->string.value));
}

ClassFunction(stdFileRead){
	u32 fSize = 0;
	u8 *buff = sd_file_read(args[0]->string.value, &fSize);
	if (buff == NULL){
		SCRIPT_FATAL_ERR("Failed to read file");
	}
	
	Vector_t vec = vecFromArray(buff, fSize, sizeof(u8));
	Variable_t v = {.variableType = ByteArrayClass, .solvedArray.vector = vec};
	return copyVariableToPtr(v);
}

ClassFunction(stdFileWrite){
	return newIntVariablePtr(sd_save_to_file(args[1]->solvedArray.vector.data, args[1]->solvedArray.vector.count, args[0]->string.value));	
}

extern int launch_payload(char *path);

ClassFunction(stdLaunchPayload){
	return newIntVariablePtr(launch_payload(args[0]->string.value));
}

int emmcFile(char *sdFile, char *sysPart, u8 mmc, u8 write){

	if (!emu_cfg.enabled && mmc == MMC_CONN_EMUMMC){
		return 1;
	}

	if (connectMMC(mmc))
		return 1;

	return DumpOrWriteEmmcPart(sdFile, sysPart, write, 1).err;
}

ClassFunction(stdEmmcFileRead){
	return newIntVariablePtr(emmcFile(args[0]->string.value, args[1]->string.value, MMC_CONN_EMMC, 0));
}

ClassFunction(stdEmmcFileWrite){
	return newIntVariablePtr(emmcFile(args[0]->string.value, args[1]->string.value, MMC_CONN_EMMC, 1));
}

ClassFunction(stdEmummcFileRead){
	return newIntVariablePtr(emmcFile(args[0]->string.value, args[1]->string.value, MMC_CONN_EMUMMC, 0));
}

ClassFunction(stdEmummcFileWrite){
	return newIntVariablePtr(emmcFile(args[0]->string.value, args[1]->string.value, MMC_CONN_EMUMMC, 1));
}

ClassFunction(stdCombinePaths){
	if (argsLen < 2 || args[0]->variableType != StringClass){
		SCRIPT_FATAL_ERR("stdCombinePaths needs 2+ args");
	}

	char *res = CpyStr(args[0]->string.value);

	for (int i = 1; i < argsLen; i++){
		if (args[i]->variableType != StringClass){
			SCRIPT_FATAL_ERR("stdCombinePaths needs 2+ args");
		}

		char *temp = CombinePaths(res, args[i]->string.value);
		free(res);
		res = temp;
	}

	return newStringVariablePtr(res, 0, 1);
}

ClassFunction(stdEscPaths){
	return newStringVariablePtr(EscapeFolder(args[0]->string.value), 0, 1);
}

ClassFunction(stdGetCwd){
	return newStringVariablePtr(CpyStr(TConf.scriptCWD), 0, 1);
}

ClassFunction(stdPower){
	power_set_state(MAX(0, (getIntValue(args[0]) % POWER_OFF_REBOOT)));
	return &emptyClass;
}

ClassFunction(stdIsPatched){
	return newIntVariablePtr(fuse_check_patched_rcm());
}

ClassFunction(stdHwType){
	return newIntVariablePtr(fuse_read_hw_type());
}

#else
#define STUBBED(name) ClassFunction(name) { return newIntVariablePtr(0); }

STUBBED(stdMountSysmmc)
STUBBED(stdMountSave)
STUBBED(stdSetPixel)
STUBBED(stdReadDir)
STUBBED(stdFileCopy)
STUBBED(stdMkdir)
STUBBED(stdGetMemUsage)
STUBBED(stdGetNcaType)
STUBBED(stdPause)
STUBBED(stdRead)
STUBBED(stdPauseMask)
STUBBED(stdColor)
STUBBED(stdMenuFull)
STUBBED(stdMountEmummc)
STUBBED(stdHasEmu)
STUBBED(stdGetMs)
STUBBED(stdClear)
STUBBED(stdRmDir)
STUBBED(stdFileExists)
STUBBED(stdFileDel)
STUBBED(stdCopyDir)
STUBBED(stdFileMove)
STUBBED(stdLaunchPayload)
STUBBED(stdFileWrite)
STUBBED(stdFileRead)
STUBBED(stdCombinePaths)
STUBBED(stdEmmcFileWrite)
STUBBED(stdEmmcFileRead)
STUBBED(stdEmummcFileRead)
STUBBED(stdEmummcFileWrite)
STUBBED(stdEscPaths)
STUBBED(stdGetCwd)
STUBBED(stdPower)
STUBBED(stdSetPrintPos)
STUBBED(stdSetPixels)
STUBBED(stdIsPatched)
STUBBED(stdHwType)
#endif

u8 oneIntoneFunction[] = { IntClass, FunctionClass };
u8 doubleFunctionClass[] = { FunctionClass, FunctionClass };
u8 threeIntsStd[] = { IntClass, IntClass, IntClass, IntClass, IntClass };
u8 twoStringArgStd[] = {StringClass, StringClass};
u8 menuArgsStd[] = {StringArrayClass, IntClass, IntArrayClass};
u8 oneStringOneByteArrayStd[] = {StringClass, ByteArrayClass};

ClassFunctionTableEntry_t standardFunctionDefenitions[] = {
	// Flow control
	{"if", stdIf, 2, oneIntoneFunction},
	{"while", stdWhile, 2, doubleFunctionClass},
	{"exit", stdExit, 0, 0},
	{"break", stdBreak, 0, 0},

	// Class creation
	{"readsave", stdMountSave, 1, twoStringArgStd},
	{"dict", stdDict, 0, 0},

	// Utils
	{"print", stdPrint, VARARGCOUNT, 0},
	{"println", stdPrintLn, VARARGCOUNT, 0},
	{"printpos", stdSetPrintPos, 2, threeIntsStd},
	{"setpixel", stdSetPixel, 3, threeIntsStd},
	{"setpixels", stdSetPixels, 5, threeIntsStd},
	{"emu", stdHasEmu, 0, 0},
	{"cwd", stdGetCwd, 0, 0},
	{"clear", stdClear, 0, 0},
	{"timer", stdGetMs, 0, 0},
	{"pause", stdPauseMask, 1, threeIntsStd},
	{"pause", stdPause, 0, 0},
	{"hidread", stdRead, 0, 0},
	{"color", stdColor, 1, threeIntsStd},
	{"menu", stdMenuFull, 3, menuArgsStd},
	{"menu", stdMenuFull, 2, menuArgsStd},
	{"power", stdPower, 1, threeIntsStd},

	// System
	{"mountsys", stdMountSysmmc, 1, twoStringArgStd},
	{"mountemu", stdMountEmummc, 1, twoStringArgStd},
	{"ncatype", stdGetNcaType, 1, twoStringArgStd},
	{"emmcread", stdEmmcFileRead, 2, twoStringArgStd},
	{"emmcwrite", stdEmmcFileWrite, 2, twoStringArgStd},
	{"emummcread", stdEmummcFileRead, 2, twoStringArgStd},
	{"emummcwrite", stdEmummcFileWrite, 2, twoStringArgStd},
	{"fuse_patched", stdIsPatched, 0, 0},
	{"fuse_hwtype", stdHwType, 0, 0},

	// FileSystem
	// 	Dir
	{"readdir", stdReadDir, 1, twoStringArgStd},
	{"deldir", stdRmDir, 1, twoStringArgStd},
	{"mkdir", stdMkdir, 1, twoStringArgStd},
	{"copydir", stdCopyDir, 2, twoStringArgStd},

	// 	File
	{"copyfile", stdFileCopy, 2, twoStringArgStd},
	{"movefile", stdFileMove, 2, twoStringArgStd},
	{"delfile", stdFileDel, 1, twoStringArgStd},
	{"readfile", stdFileRead, 1, twoStringArgStd},
	{"writefile", stdFileWrite, 2, oneStringOneByteArrayStd},
	
	// 	Utils
	{"fsexists", stdFileExists, 1, twoStringArgStd},
	{"payload", stdLaunchPayload, 1, twoStringArgStd},
	{"combinepath", stdCombinePaths, VARARGCOUNT, 0},
	{"escapepath", stdEscPaths, 1, twoStringArgStd},
};

ClassFunctionTableEntry_t* searchStdLib(char* funcName, u8 *len) {
	u8 lenInternal = 0;
	*len = 0;
	ClassFunctionTableEntry_t *ret = NULL;

	for (int i = 0; i < ARRAY_SIZE(standardFunctionDefenitions); i++) {
		if (!strcmp(funcName, standardFunctionDefenitions[i].name)) {
			lenInternal++;
			if (ret == NULL){
				ret = &standardFunctionDefenitions[i];
			}
		}
		else if (lenInternal != 0){
			*len = lenInternal;
			return ret;
		}
	}

	*len = lenInternal;
	return ret;
}
