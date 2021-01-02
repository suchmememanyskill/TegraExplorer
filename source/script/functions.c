#include "types.h"
#include "args.h"
#include "variables.h"
#include <string.h>
#include "../gfx/gfx.h"
#include <mem/heap.h>
#include "lexer.h"

#include <storage/nx_sd.h>
#include "../fs/fsutils.h"
#include "../gfx/gfxutils.h"
#include "../hid/hid.h"
#include <utils/util.h>

#define scriptFunction(name) Variable_t name(scriptCtx_t *ctx, Variable_t *vars, u32 varLen)

scriptFunction(funcIf) {
	setCurIndentInstruction(ctx, (vars[0].integerType == 0), 0, -1);
	return NullVar;
}

scriptFunction(funcPrint) {
	for (u32 i = 0; i < varLen; i++) {
		if (vars[i].varType == IntType)
			gfx_printf("%d", vars[i].integerType);
		else if (vars[i].varType == StringType)
			gfx_printf("%s", vars[i].stringType);
		else if (vars[i].varType == IntArrayType) {
			gfx_printf("[");
			int* v = vecGetArray(int*, vars[i].vectorType);
			for (u32 j = 0; j < vars[i].vectorType.count; j++)
				gfx_printf((j + 1 == vars[i].vectorType.count) ? "%d" : "%d, ", v[j]);
			gfx_printf("]");
		}
	}

	return NullVar;
}

scriptFunction(funcPrintln) {
	funcPrint(ctx, vars, varLen);
	gfx_printf("\n");
	return NullVar;
}

scriptFunction(funcWhile) {
	setCurIndentInstruction(ctx, (vars[0].integerType == 0), 0, ctx->startEquation);
	
	return NullVar;
}

scriptFunction(funcElse) {
	indentInstructor_t* curInstruction = getCurIndentInstruction(ctx);
	setCurIndentInstruction(ctx, !curInstruction->skip, 0, -1);
	return NullVar;
}

scriptFunction(funcLen) {
	if (vars[0].varType >= IntArrayType && vars[0].varType <= ByteArrayType)
		return IntVal(vars[0].vectorType.count);
	else if (vars[0].varType == StringType) {
		if (vars[0].stringType != NULL)
			return IntVal(strlen(vars[0].stringType));
	}

	return ErrVar(ERRINVALIDTYPE);
}

scriptFunction(funcMakeByteArray){
	u8 *buff = malloc(vars[0].vectorType.count);
	vecDefArray(int*, entries, vars[0].vectorType);
	for (int i = 0; i < vars[0].vectorType.count; i++)
		buff[i] = (u8)(entries[i] & 0xFF);

	Vector_t v = vecFromArray(buff, vars[0].vectorType.count, sizeof(u8));
	return newVar(ByteArrayType, 1, .vectorType = v);
}

scriptFunction(funcSetPixel){
	u32 color = 0xFF000000 | ((vars[2].integerType & 0xFF) << 16) | ((vars[3].integerType & 0xFF) << 8) | (vars[4].integerType & 0xFF);
	gfx_set_pixel_horz(vars[0].integerType, vars[1].integerType, color);
	return NullVar;
}

scriptFunction(funcFileExists){
	return newVar(IntType, 0, FileExists(vars[0].stringType));
}

// Args: Str (Path)
scriptFunction(funcReadFile){
	u32 fSize = 0;
	u8 *buff = sd_file_read(vars[0].stringType, &fSize);
	if (buff == NULL)
		return ErrVar(ERRFATALFUNCFAIL);
	
	Vector_t vec = vecFromArray(buff, fSize, sizeof(u8));
	return newVar(ByteArrayType, 1, .vectorType = vec);
}

// Args: Str (Path), vector(Byte) (toWrite) 
scriptFunction(funcWriteFile){
	return newVar(IntType, 0, sd_save_to_file(vars[1].vectorType.data, vars[1].vectorType.count, vars[0].stringType));
}

// Args: vector(Byte)
scriptFunction(funcByteToStr){
	char *str = calloc(vars[0].vectorType.count + 1, 1);
	memcpy(str, vars[0].vectorType.data, vars[0].vectorType.count);
	return newVar(StringType, 1, .stringType = str);
}

scriptFunction(funcReturn){
	if (ctx->indentIndex > 0){
		vecDefArray(indentInstructor_t*, instructors, ctx->indentInstructors);
		for (int i = ctx->indentIndex - 1; i >= 0; i--){
			indentInstructor_t ins = instructors[i];
			if (ins.active && ins.jump && ins.function){
				ctx->curPos = ins.jumpLoc - 1;
				ins.active = 0;
				ctx->indentIndex = i;
				break;
			}
		}
	}

	return NullVar;
}

scriptFunction(funcExit){
	return ErrVar(ERRESCSCRIPT);
}

/*
scriptFunction(funcContinue){
	if (ctx->indentIndex > 0){
		vecDefArray(indentInstructor_t*, instructors, ctx->indentInstructors);
		for (int i = ctx->indentIndex - 1; i >= 0; i--){
			indentInstructor_t ins = instructors[i];
			if (ins.active && ins.jump && !ins.function){
				ctx->curPos = ins.jumpLoc - 1;
				ins.active = 0;
				ctx->indentIndex = i;
				break;
			}
		}
	}

	return NullVar;
}
*/

// Args: Int, Int
scriptFunction(funcSetPrintPos){
	if (vars[0].integerType > 78 || vars[0].integerType < 0 || vars[1].integerType > 42 || vars[1].integerType < 0)
		return ErrVar(ERRFATALFUNCFAIL);
	
	gfx_con_setpos(vars[0].integerType * 16, vars[1].integerType * 16);
	return NullVar;
}

scriptFunction(funcClearScreen){
	gfx_clearscreen();
	return NullVar;
}

int validRanges[] = {
	1279,
	719,
	1279,
	719
};

// Args: Int, Int, Int, Int, Int
scriptFunction(funcDrawBox){
	for (int i = 0; i < ARR_LEN(validRanges); i++){
		if (vars[i].integerType > validRanges[i] || vars[i].integerType < 0)
			return ErrVar(ERRFATALFUNCFAIL);
	}

	gfx_box(vars[0].integerType, vars[1].integerType, vars[2].integerType, vars[3].integerType, vars[4].integerType);
	return NullVar;
}

typedef struct {
	char *name;
	u32 color;
} ColorCombo_t;

ColorCombo_t combos[] = {
	{"RED", COLOR_RED},
	{"ORANGE", COLOR_ORANGE},
	{"YELLOW", COLOR_YELLOW},
	{"GREEN", COLOR_GREEN},
	{"BLUE", COLOR_BLUE},
	{"VIOLET", COLOR_VIOLET},
	{"WHITE", COLOR_WHITE}
};

// Args: Str
scriptFunction(funcSetColor){
	for (int i = 0; i < ARR_LEN(combos); i++){
		if (!strcmp(combos[i].name, vars[0].stringType)){
			SETCOLOR(combos[i].color, COLOR_DEFAULT);
			break;
		}
	}

	return NullVar;
}

scriptFunction(funcPause){
	return newVar(IntType, 0, hidWait()->buttons);
}

// Args: Int
scriptFunction(funcWait){
	u32 timer = get_tmr_ms();
	while (timer + vars[0].integerType > get_tmr_ms()){
		gfx_printf("<Wait %d seconds> \r", (vars[0].integerType - (get_tmr_ms() - timer)) / 1000);
	}
	gfx_putc('\n');
	return NullVar;
}

u8 fiveInts[] = {IntType, IntType, IntType, IntType, IntType};
u8 singleIntArray[] = { IntArrayType };
u8 singleInt[] = { IntType };
u8 singleAny[] = { varArgs };
u8 singleStr[] = { StringType };
u8 singleByteArr[] = { ByteArrayType };
u8 StrByteVec[] = { StringType, ByteArrayType};

functionStruct_t scriptFunctions[] = {
	{"if", funcIf, 1, singleInt},
	{"print", funcPrint, varArgs, NULL},
	{"println", funcPrintln, varArgs, NULL},
	{"while", funcWhile, 1, singleInt},
	{"else", funcElse, 0, NULL},
	{"len", funcLen, 1, singleAny},
	{"byte", funcMakeByteArray, 1, singleIntArray},
	{"setPixel", funcSetPixel, 5, fiveInts},
	{"fileRead", funcReadFile, 1, singleStr},
	{"fileWrite", funcWriteFile, 2, StrByteVec},
	{"fileExists", funcFileExists, 1, singleStr},
	{"bytesToStr", funcByteToStr, 1, singleByteArr},
	{"return", funcReturn, 0, NULL},
	{"exit", funcExit, 0, NULL},
	//{"continue", funcContinue, 0, NULL},
	{"printPos", funcSetPrintPos, 2, fiveInts},
	{"clearscreen", funcClearScreen, 0, NULL},
	{"drawBox", funcDrawBox, 5, fiveInts},
	{"color", funcSetColor, 1, singleStr},
	{"pause", funcPause, 0, NULL},
	{"wait", funcWait, 1, singleInt},
};

Variable_t executeFunction(scriptCtx_t* ctx, char* func_name, lexarToken_t *start, u32 len) {
	Vector_t args = { 0 };

	if (len > 0) {
		args = extractVars(ctx, start, len);
		Variable_t* vars = vecGetArray(Variable_t*, args);
		for (int i = 0; i < args.count; i++) {
			if (vars[i].varType == ErrType)
				return vars[i];
		}
	}

	Variable_t* vars = vecGetArray(Variable_t*, args);

	for (u32 i = 0; i < ARRAY_SIZE(scriptFunctions); i++) {
		if (scriptFunctions[i].argCount == args.count || scriptFunctions[i].argCount == varArgs) {
			if (!strcmp(scriptFunctions[i].key, func_name)) {
				if (scriptFunctions[i].argCount != varArgs && scriptFunctions[i].argCount != 0) {
					u8 argsMatch = 1;
					for (u32 j = 0; j < args.count; j++) {
						if (vars[j].varType != scriptFunctions[i].typeArray[j] && scriptFunctions[i].typeArray[j] != varArgs) {
							argsMatch = 0;
							break;
						}
					}

					if (!argsMatch)
						continue;
				}

				Variable_t ret = scriptFunctions[i].value(ctx, vars, args.count);
				freeVariableVector(&args);
				return ret;
			}
		}
	}

	Variable_t* var = dictVectorFind(&ctx->varDict, func_name);
	if (var != NULL) {
		if (var->varType == JumpType) {
			setCurIndentInstruction(ctx, 0, 1, ctx->curPos);
			ctx->curPos = var->integerType - 1;
			return NullVar;
		}
	}

	return ErrVar(ERRNOFUNC);
}