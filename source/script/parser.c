#include "args.h"
#include "types.h"
#include "variables.h"
#include "lexer.h"
#include "../gfx/gfx.h"
#include "../utils/utils.h"
#include <mem/heap.h>

#define scriptResultCreate(resCode, nearToken) (scriptResult_t) {resCode, nearToken, 1}
#define scriptResultCreateLen(resCode, nearToken, len) (scriptResult_t) {resCode, nearToken, len}

scriptResult_t runFunction(scriptCtx_t* ctx, u32 len) {
	lexarToken_t* tokens = vecGetArray(lexarToken_t*, ctx->script);
	Variable_t res = solveEquation(ctx, &tokens[ctx->startEquation], len, 1);

	if (res.varType == ErrType) {
		return scriptResultCreateLen(res.integerType, &tokens[ctx->startEquation], len);
	}

	return scriptResultCreate(0, 0);
}

#define RUNFUNCWITHPANIC(ctx, len) scriptResult_t res = runFunction(ctx, len); if (res.resCode != 0) return res

static inline int checkIfVar(u8 token) {
	return (token == StrLit || token == IntLit || token == Variable || token == RSBracket || token == ArrayVariable);
}

scriptResult_t mainLoop(scriptCtx_t* ctx) {
	ctx->startEquation = 0;
	lexarToken_t* lexArr = vecGetArray(lexarToken_t*, ctx->script);
	for (ctx->curPos = 0; ctx->curPos < ctx->script.count; ctx->curPos++) {
		u32 i = ctx->curPos;
		lexarToken_t curToken = lexArr[i];

		if (curToken.token == EquationSeperator) {
			RUNFUNCWITHPANIC(ctx, ctx->curPos - ctx->startEquation);
			ctx->startEquation = ctx->curPos + 1;
		}
		else if (curToken.token == FunctionAssignment) {
			setCurIndentInstruction(ctx, 1, 0, -1);
			dict_t x = newDict(CpyStr(curToken.text), newVar(JumpType, 0, .integerType = ctx->curPos + 1));
			vecAddElement(&ctx->varDict, x);
		}
		else if (curToken.token == LCBracket) {
			indentInstructor_t* ins = getCurIndentInstruction(ctx);
			if (ins->active) {
				if (ins->skip) {
					int distance = distanceBetweenTokens(&lexArr[i + 1], ctx->script.count - i - 1, LCBracket, RCBracket);
					if (distance < 0)
						return scriptResultCreate(ERRSYNTAX, &lexArr[i]);
					ctx->curPos += distance + 1;
				}
				else
					ctx->indentIndex++;
			}
			else
				return scriptResultCreate(ERRINACTIVEINDENT, &lexArr[i]);

			ctx->startEquation = ctx->curPos + 1;
		}
		else if (curToken.token == RCBracket) {
			ctx->indentIndex--;
			indentInstructor_t* ins = getCurIndentInstruction(ctx);
			if (ins->active && ins->jump) {
				ctx->curPos = ins->jumpLoc - 1;
			}

			ins->active = 0;
			ctx->startEquation = ctx->curPos + 1;
		}
	}

	return scriptResultCreate(0, 0);
}

void printToken(lexarToken_t* token) {
	switch (token->token) {
		case Variable:
		case VariableAssignment:
		case Function:
		case FunctionAssignment:
		case ArrayVariable:
		case ArrayVariableAssignment:
			gfx_printf("%s", token->text);
			break;
		case StrLit:
			//printf("%d: '%s'\n", vec.tokens[i].token, vec.tokens[i].text);
			gfx_printf("\"%s\"", token->text);
			break;
		case IntLit:
			//printf("%d: %d\n", vec.tokens[i].token, vec.tokens[i].val);
			gfx_printf("%d", token->val);
			break;
		default:
			//printf("%d: %c\n", vec.tokens[i].token, lexarDebugGetTokenC(vec.tokens[i].token));
			gfx_printf("%c", lexarDebugGetTokenC(token->token));
			break;
	}
}

void printError(scriptResult_t res) {
	if (res.resCode) {
		gfx_printf("Error %d found!\nNear: ", res.resCode);
		for (int i = 0; i < res.len; i++) {
			printToken(&res.nearToken[i]);
		}
	}
}