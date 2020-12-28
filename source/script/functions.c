#include "types.h"
#include "args.h"
#include "variables.h"
#include <string.h>
#include "../gfx/gfx.h"
#include <mem/heap.h>
#include "lexer.h"

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

u8 fiveInts[] = {IntType, IntType, IntType, IntType, IntType};
u8 singleIntArray[] = { IntArrayType };
u8 singleInt[] = { IntType };
u8 singleAny[] = { varArgs };

functionStruct_t scriptFunctions[] = {
	{"if", funcIf, 1, singleInt},
	{"print", funcPrint, varArgs, NULL},
	{"println", funcPrintln, varArgs, NULL},
	{"while", funcWhile, 1, singleInt},
	{"else", funcElse, 0, NULL},
	{"len", funcLen, 1, singleAny},
	{"byte", funcMakeByteArray, 1, singleIntArray},
	{"setPixel", funcSetPixel, 5, fiveInts},
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