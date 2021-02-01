#pragma once
#include "types.h"

void dictVectorAdd(Vector_t* v, dict_t add);
Variable_t* dictVectorFind(Vector_t* v, const char* key);
void freeDictVector(Vector_t* v);
void freeVariableVector(Vector_t* v);
void freeVariable(Variable_t dv);
scriptCtx_t createScriptCtx();
Variable_t copyVariable(Variable_t copy);

u8 setIndentInstruction(scriptCtx_t* ctx, u8 level, u8 skip, u8 func, int jumpLoc);
indentInstructor_t* getCurIndentInstruction(scriptCtx_t* ctx);

static inline u8 setCurIndentInstruction(scriptCtx_t* ctx, u8 skip, u8 func, int jumpLoc) {
	return setIndentInstruction(ctx, ctx->indentIndex, skip, func, jumpLoc);
}