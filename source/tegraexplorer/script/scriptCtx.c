#include "scriptCtx.h"
#include "list.h"
#include "malloc.h"
#include "types.h"

scriptCtx_t createScriptCtx() {
	scriptCtx_t ctx;

	ctx.indentInstructors = calloc(sizeof(indentInstructor_t), 64);
	ctx.indentLevel = 0;
	ctx.vars = varVectorInit(16);
	ctx.script = NULL;
	ctx.lastToken.token = 0;
	ctx.varToken.token = 0;
	ctx.startEq = 0;
	ctx.varIndexStruct = 0;

	return ctx;
}

void destroyScriptCtx(scriptCtx_t* ctx) {
	// TODO
	varVectorFree(&ctx->vars);
	free(ctx->indentInstructors);
}

u8 setIndentInstruction(scriptCtx_t* ctx, u8 level, u8 skip, int jumpLoc) {
	if (level >= 64)
		return 1;

	ctx->indentInstructors[level].skip = skip;
	ctx->indentInstructors[level].active = 1;
	ctx->indentInstructors[level].jump = (jumpLoc >= 0) ? 1 : 0;
	ctx->indentInstructors[level].jumpLoc = jumpLoc;

	return 0;
}