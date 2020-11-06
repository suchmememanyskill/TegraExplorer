#include "functions.h"
#include "list.h"
#include "args.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "scriptCtx.h"
#include "../../gfx/gfx.h"
#include "../../mem/heap.h"
#include <string.h>
#include "../../hid/hid.h"

dictValue_t funcPrint(scriptCtx_t* ctx, varVector_t* args) {
	for (int i = 0; i < args->stored; i++) {
		switch (args->vars[i].value.varType) {
		case IntType:
			gfx_printf("%d", args->vars[i].value.integer);
			break;
		case StringType:
			gfx_printf("%s", args->vars[i].value.string);
			break;
		}
	}

	return NullDictValue();
}

dictValue_t funcPrintln(scriptCtx_t* ctx, varVector_t* args) {
	funcPrint(ctx, args);
	gfx_printf("\n");
	return NullDictValue();
}

dictValue_t funcIf(scriptCtx_t* ctx, varVector_t* args) {
	if (args->vars[0].value.varType == IntType) {
		setCurIndentInstruction(ctx, (args->vars[0].value.integer == 0), -1);
		return NullDictValue();
	}

	return ErrDictValue(ERRINVALIDTYPE);
}

dictValue_t scriptElse(scriptCtx_t* ctx, varVector_t* args) {
	indentInstructor_t curInstruction = getCurIndentInstruction(ctx);
	setCurIndentInstruction(ctx, !curInstruction.skip, -1);
	return NullDictValue();
}

dictValue_t scriptWhile(scriptCtx_t* ctx, varVector_t* args) {
	if (args->vars[0].value.varType == IntType) {
		setCurIndentInstruction(ctx, (args->vars[0].value.integer == 0), ctx->lastTokenPos);
		return NullDictValue();
	}

	return ErrDictValue(ERRINVALIDTYPE);
}

dictValue_t scriptLen(scriptCtx_t* ctx, varVector_t* args) {
	if (args->vars[0].value.varType >= IntArrayType && args->vars[0].value.varType <= ByteArrayType)
		return IntDictValue(args->vars[0].value.arrayLen);
	else if (args->vars[0].value.varType == StringType) {
		if (args->vars[0].value.string != NULL)
			return IntDictValue(strlen(args->vars[0].value.string));
	}

	return ErrDictValue(ERRINVALIDTYPE);
}

dictValue_t scriptMakeByteArray(scriptCtx_t* ctx, varVector_t* args) {
	if (args->vars[0].value.varType == IntArrayType) {
		u8* buff = calloc(args->vars[0].value.arrayLen, 1);
		for (int i = 0; i < args->vars[0].value.arrayLen; i++) {
			buff[i] = (u8)(args->vars[0].value.integerArray[i] & 0xFF);
		}

		return DictValueCreate(ByteArrayType | FREEINDICT, args->vars[0].value.arrayLen, buff);
	}

	return ErrDictValue(ERRINVALIDTYPE);
}

dictValue_t scriptWait(scriptCtx_t*ctx, varVector_t* args){
	Inputs* input = hidWait();
	
	return IntDictValue(input->buttons);
}

dictValue_t scriptSetPixel(scriptCtx_t *ctx, varVector_t* args){
	int pixelArgs[5] = {0};

	for (int i = 0; i < args->stored; i++){
		if (args->vars[i].value.varType != IntType)
			return ErrDictValue(ERRINVALIDTYPE);
		pixelArgs[i] = args->vars[i].value.integer;
	}

	u32 color = 0xFF000000 | ((pixelArgs[2] & 0xFF) << 16) | ((pixelArgs[3] & 0xFF) << 8) | ((pixelArgs[4] & 0xFF));

	gfx_setPixel(pixelArgs[0], pixelArgs[1], color);
	return NullDictValue();
}

const static str_fnc_struct functions[] = {
	{"print", funcPrint, {0x40}},
	{"println", funcPrintln, {0x40}},
	{"if", funcIf, {1}},
	{"while", scriptWhile, {1}},
	{"else", scriptElse, {0}},
	{"len", scriptLen, {1}},
	{"byte", scriptMakeByteArray, {1}},
	{"wait", scriptWait, {0}},
	{"setPixel", scriptSetPixel, {5}},
	{NULL, NULL, {0}},
};

varVector_t extractVars(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len) {
	varVector_t args = varVectorInit(4);
	int lastLoc = 0;
	for (int i = 0; i < len; i++) {
		if (tokens[i].token == LSBracket) {
			int distance = distanceBetweenTokens(&tokens[i + 1], len - i - 1, LSBracket, RSBracket);
			i += distance + 1;
		}
		if (tokens[i].token == Seperator) {
			dictValue_t res = solveEquation(ctx, &tokens[lastLoc], i - lastLoc);
			lastLoc = i + 1;
			varVectorAdd(&args, DictCreate(NULL, res));
		}

		if (i + 1 >= len) {
			dictValue_t res = solveEquation(ctx, &tokens[lastLoc], i + 1 - lastLoc);
			varVectorAdd(&args, DictCreate(NULL, res));
		}
	}

	return args;
}

dictValue_t executeFunction(scriptCtx_t* ctx, char* func_name) {
	int argCount = 0;
	varVector_t args = { 0 };

	if (ctx->args_loc.stored > 0) {
		args = extractVars(ctx, ctx->args_loc.tokens, ctx->args_loc.stored);

		for (int i = 0; i < args.stored; i++) {
			if (args.vars[i].value.varType == ErrType)
				return args.vars[i].value;
		}

		argCount = args.stored;
	}

	for (u32 i = 0; functions[i].key != NULL; i++) {
		if (!strcmp(functions[i].key, func_name)) {
			if (argCount != functions[i].argCount && !functions[i].varArgs)
				continue;

			dictValue_t val = functions[i].value(ctx, &args);
			// free args
			varVectorFree(&args);
			return val;
		}
	}

	dictValue_t* var = varVectorFind(&ctx->vars, func_name);
	if (var != NULL) {
		if (var->type == JumpType) {
			setCurIndentInstruction(ctx, 0, ctx->curPos);
			ctx->curPos = var->integer - 1;
			ctx->lastToken.token = Invalid;
			return NullDictValue();
		}
	}

	return ErrDictValue(ERRNOFUNC);
}