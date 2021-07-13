#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "eval.h"
#include "garbageCollector.h"
#include "intClass.h"
#include "standardLibrary.h"
#include "scriptError.h"
#include <string.h>

#ifndef WIN32
#include "../storage/mountmanager.h"
#include "../keys/keys.h"
#endif

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

ClassFunction(stdPrint) {
	for (int i = 0; i < argsLen; i++) {
		Variable_t* res = callMemberFunctionDirect(args[i], "print", NULL, 0);
		if (res == NULL)
			return NULL;
	}
	

	return &emptyClass;
}

ClassFunction(stdExit) {
	return NULL;
}

ClassFunction(stdBreak) {
	scriptLastError = SCRIPT_BREAK;
	return NULL;
}

ClassFunction(stdDict) {
	Variable_t a = { 0 };
	a.variableType = DictionaryClass;
	a.dictionary.vector = newVec(sizeof(Dict_t), 0);
	return copyVariableToPtr(a);
}

#ifndef WIN32
ClassFunction(stdMountSysmmc){
	if (connectMMC(MMC_CONN_EMMC))
		return newIntVariablePtr(1);

	Variable_t *arg = (*args);
	if (mountMMCPart(arg->string.value).err)
		return newIntVariablePtr(1); // Maybe change for error?

	return newIntVariablePtr(0);
}

ClassFunction(stdMountSave){
	Variable_t *arg = (*args);
	Variable_t var = {.variableType = SaveClass};
	SaveClass_t* save = calloc(1, sizeof(SaveClass_t));
	if (f_open(&save->saveFile, arg->string.value, FA_READ))
		return NULL;
	save_init(&save->saveCtx, &save->saveFile, dumpedKeys.save_mac_key, 0);
	if (!save_process(&save->saveCtx))
		return NULL;
	
	var.save = save;
	return copyVariableToPtr(var);
}
ClassFunction(stdSetPixel) {
	u32 color = getIntValue(args[2]);
	gfx_set_pixel_horz(args[0]->integer.value, args[1]->integer.value, color);
	return &emptyClass;
}
#else
ClassFunction(stdMountSysmmc){
	return newIntVariablePtr(0);
}
ClassFunction(stdMountSave){
	return newIntVariablePtr(0);
}
ClassFunction(stdSetPixel) {
	return newIntVariablePtr(0);
}
#endif

enum standardFunctionIndexes {
	STD_IF = 0,
	STD_WHILE,
	STD_PRINT,
	STD_MOUNTSYSMMC,
	STD_MOUNTSAVE,
	STD_EXIT,
	STD_BREAK,
	STD_DICT,
	STD_SETPIXEL,
};

u8 oneIntoneFunction[] = { IntClass, FunctionClass };
u8 doubleFunctionClass[] = { FunctionClass, FunctionClass };
u8 oneStringArgStd[] = {StringClass};
u8 threeIntsStd[] = { IntClass, IntClass, IntClass };

ClassFunctionTableEntry_t standardFunctionDefenitions[] = {
	[STD_IF] = {"if", stdIf, 2, oneIntoneFunction},
	[STD_WHILE] = {"while", stdWhile, 2, doubleFunctionClass},
	[STD_PRINT] = {"print", stdPrint, VARARGCOUNT, 0},
	[STD_MOUNTSYSMMC] = {"mountsys", stdMountSysmmc, 1, oneStringArgStd},
	[STD_MOUNTSAVE] = {"readsave", stdMountSave, 1, oneStringArgStd},
	[STD_EXIT] = {"exit", stdExit, 0, 0},
	[STD_BREAK] = {"break", stdBreak, 0, 0},
	[STD_DICT] = {"dict", stdDict, 0, 0},
	[STD_SETPIXEL] = {"setpixel", stdSetPixel, 3, threeIntsStd},
};

ClassFunctionTableEntry_t* searchStdLib(char* funcName) {
	for (int i = 0; i < ARRAY_SIZE(standardFunctionDefenitions); i++) {
		if (!strcmp(funcName, standardFunctionDefenitions[i].name))
			return &standardFunctionDefenitions[i];
	}

	return NULL;
}