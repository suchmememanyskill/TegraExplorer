#pragma once
#include "types.h"

#define TEXTHOLDEREMPTY() (textHolder_t) {0}
#define TEXTHOLDER(start, len) (textHolder_t) {start, len, 1}

u8 setIndentInstruction(scriptCtx_t* ctx, u8 level, u8 skip, int jumpLoc);
void destroyScriptCtx(scriptCtx_t* ctx);
scriptCtx_t createScriptCtx();
static inline indentInstructor_t getCurIndentInstruction(scriptCtx_t* ctx);
static inline u8 setCurIndentInstruction(scriptCtx_t* ctx, u8 skip, int jumpLoc);

static inline u8 setCurIndentInstruction(scriptCtx_t* ctx, u8 skip, int jumpLoc) {
	return setIndentInstruction(ctx, ctx->indentLevel, skip, jumpLoc);
}

static inline indentInstructor_t getCurIndentInstruction(scriptCtx_t* ctx) {
	return ctx->indentInstructors[ctx->indentLevel];
}