#include "args.h"
#include "parser.h"
#include "types.h"
#include "list.h"
#include "functions.h"
#include "scriptCtx.h"
#include "lexer.h"
#include <string.h>
#include "../../mem/heap.h"
#include "../utils/utils.h"
#include "../../storage/nx_sd.h"
#include "../../gfx/gfx.h"
#include "../../hid/hid.h"
#include "../gfx/gfxutils.h"

//#define DPRINTF(str, ...) printf(str, __VA_ARGS__)
#define DPRINTF 

#define findNextCharsCtx(ctx, targets) findNextChars(&(ctx->curPos), targets)

int checkIfVar(u16 token) {
    return (token == StrLit || token == IntLit || token == Variable || token == RSBracket);
}

void printToken(lexarToken_t* token) {
    switch (token->token) {
    case 1:
        gfx_printf("%s ", token->text);
        break;
    case 6:
        //printf("%d: '%s'\n", vec.tokens[i].token, vec.tokens[i].text);
        gfx_printf("\"%s\" ", token->text);
        break;
    case 7:
        //printf("%d: %d\n", vec.tokens[i].token, vec.tokens[i].val);
        gfx_printf("%d ", token->val);
        break;
    default:
        //printf("%d: %c\n", vec.tokens[i].token, lexarDebugGetTokenC(vec.tokens[i].token));
        gfx_printf("%c ", lexarDebugGetTokenC(token->token));
        break;
    }
}

scriptResult_t runFunction(scriptCtx_t* ctx) {
    dictValue_t res = solveEquation(ctx, &ctx->script->tokens[ctx->startEq], ctx->curPos - ctx->startEq);

    if (res.varType == ErrType)
        return scriptResultCreate(res.integer, &ctx->script->tokens[ctx->startEq]);

    ctx->startEq = ctx->curPos;
    if (ctx->varToken.token != Invalid) { // we should not assign null's
        ctx->varToken.token = Invalid;
        if (ctx->varIndexSet) {
            ctx->varIndexSet = 0;
            dictValue_t* arrayVal = varVectorFind(&ctx->vars, ctx->varToken.text);
            if (!(arrayVal == NULL || arrayVal->arrayLen <= ctx->varIndex)) {
                if (arrayVal->varType == IntArrayType && res.varType == IntType) {
                    arrayVal->integerArray[ctx->varIndex] = res.integer;
                }
                else if (arrayVal->varType == StringArrayType && res.varType == StringType) {
                    free(arrayVal->stringArray[ctx->varIndex]);
                    arrayVal->stringArray[ctx->varIndex] = res.string;
                }
                else if (arrayVal->varType == ByteArrayType && res.varType == IntType) {
                    arrayVal->byteArray[ctx->varIndex] = (u8)(res.integer & 0xFF);
                }
                else {
                    return scriptResultCreate(ERRINVALIDTYPE, &ctx->script->tokens[ctx->startEq]);
                }
            }
            else {
                return scriptResultCreate(ERRNOVAR, &ctx->script->tokens[ctx->startEq]);
            }
        }
        else {
            varVectorAdd(&ctx->vars, DictCreate(util_cpyStr(ctx->varToken.text), res));
        }
    }
    else {
        freeDictValueOnVar(res);
    }

    return scriptResultCreate(0, 0);
}

// TODO
// Make sure mem gets free'd properly
// Fix bug that custom functions cannot be run from end of script
// add arrays

#define RUNFUNCWITHPANIC(ctx) scriptResult_t res = runFunction(ctx); if (res.resCode != 0) return res

scriptResult_t mainLoop(scriptCtx_t* ctx) {
    for (ctx->curPos = 0; ctx->curPos < ctx->script->stored; ctx->curPos++) {
        u32 i = ctx->curPos;
        lexarToken_t curToken = ctx->script->tokens[i];
        //printToken(&curToken);

        if (checkIfVar(ctx->lastToken.token) && curToken.token == Variable) {
            RUNFUNCWITHPANIC(ctx);
        }
        else if (curToken.token == Equal) {
            // Set last var to lastToken, if lastToken is var

            if (ctx->lastToken.token == RSBracket) {
                int layer = 0;
                i = ctx->lastTokenPos - 1;
                for (; i > 0; i--) {
                    if (ctx->script->tokens[i].token == RSBracket)
                        layer++;
                    else if (ctx->script->tokens[i].token == LSBracket) {
                        if (layer == 0)
                            break;
                        layer--;
                    }
                }

                i--;

                if (ctx->script->tokens[i].token == Variable) {
                    dictValue_t index = solveEquation(ctx, &ctx->script->tokens[i + 2], ctx->lastTokenPos - i - 2);
                    ctx->lastTokenPos = i;
                    ctx->lastToken = ctx->script->tokens[i];

                    if (index.varType == IntType) {
                        ctx->varIndex = index.integer;
                        ctx->varIndexSet = 1;

                        if (ctx->varIndexSet) {
                            dictValue_t* arrayVal = varVectorFind(&ctx->vars, ctx->varToken.text);
                            if (arrayVal == NULL) {
                                return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);
                            }
                           if (ctx->varIndex < 0 || ctx->varIndex >= arrayVal->arrayLen || arrayVal->varType < IntArrayType)
                               return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);
                        }
                    }
                    else
                        return scriptResultCreate(ERRINVALIDTYPE, &ctx->script->tokens[ctx->curPos]);
                }
                else 
                    return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);
            }

            if (ctx->lastToken.token == Variable) {
                ctx->startEq = ctx->curPos + 1;
                ctx->varToken = ctx->lastToken;
            }
            else
                return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);

            //printf("Setting token %s to next assignment", ctx->lastToken.text);

            // !! check prev for ] for arrays
        }
        else if (curToken.token == LBracket) {
            i++;
            int distance = distanceBetweenTokens(&ctx->script->tokens[i], ctx->script->stored - i, LBracket, RBracket);
            if (distance < 0)
                return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);

            i += distance;
            ctx->curPos = i;

            if (ctx->curPos + 1 >= ctx->script->stored && checkIfVar(ctx->lastToken.token)) {
                solveEquation(ctx, &ctx->script->tokens[ctx->startEq], i + 1 - ctx->startEq);
            }
            continue;
        }
        else if (curToken.token == LCBracket) {
            if (ctx->lastToken.token == Equal && ctx->varToken.token == Variable) {
                varVectorAdd(&ctx->vars, DictCreate(util_cpyStr(ctx->varToken.text), DictValueCreate(JumpType, 0, ctx->curPos)));
                ctx->varToken.token = Invalid;
                setCurIndentInstruction(ctx, 1, -1);
            }

            if (checkIfVar(ctx->lastToken.token)) {
                RUNFUNCWITHPANIC(ctx);
            }

            ctx->startEq = ctx->curPos + 1;

            indentInstructor_t ins = getCurIndentInstruction(ctx);
            if (ins.active) {
                if (ins.skip) {
                    int distance = distanceBetweenTokens(&ctx->script->tokens[i + 1], ctx->script->stored - i - 1, LCBracket, RCBracket);
                    if (distance < 0)
                        return scriptResultCreate(ERRSYNTAX, &ctx->script->tokens[ctx->curPos]);
                    ctx->curPos += distance + 1;
                    ctx->startEq = ctx->curPos + 1;
                }
                else {
                    ctx->indentLevel++;
                }
            }
            else
                return scriptResultCreate(ERRINACTIVEINDENT, &ctx->script->tokens[ctx->curPos]);
        }
        else if (curToken.token == RCBracket) {
            if (checkIfVar(ctx->lastToken.token)) {
                RUNFUNCWITHPANIC(ctx);
                if (i != ctx->curPos)
                    continue;
            }

            ctx->indentLevel--;

            indentInstructor_t ins = getCurIndentInstruction(ctx);
            if (ins.active && ins.jump) {
                ctx->curPos = ins.jumpLoc - 1;
            }

            ctx->startEq = ctx->curPos + 1;
        }
        else if (ctx->curPos + 1 >= ctx->script->stored && checkIfVar(ctx->lastToken.token)) {
            solveEquation(ctx, &ctx->script->tokens[ctx->startEq], i + 1 - ctx->startEq);
        }

        ctx->lastToken = ctx->script->tokens[ctx->curPos];
        ctx->lastTokenPos = ctx->curPos;
    }

    return scriptResultCreate(0, 0);
}

const char* functionFails[] = {
    "An invalid operation was performed",
    "Double Nots are not allowed",
    "A syntax error was found",
    "The recieved type was incorrect",
    "The variable could not be found",
    "The specified function could not be found",
    "An inactive indent was found"
};

void startScript(char* path) {
    char* file = sd_file_read(path, NULL);

    if (file == NULL)
        return;

    gfx_clearscreen();

    scriptCtx_t ctx = createScriptCtx();
    lexarVector_t vec = lexarGo(file);
    free(file);
    ctx.script = &vec;

    scriptResult_t res = mainLoop(&ctx);
    gfx_printf("end of script\n");

    if (res.resCode) {
        gfx_printf("[ERROR] %d\n%s\nNear:", res.resCode, functionFails[res.resCode - 1]);
        printToken(res.nearToken);
    }
   
    gfx_printf("\n\n");
    //mainLoop2(&ctx, &vec);
    lexarVectorClear(&vec);
    destroyScriptCtx(&ctx);
    hidWait();
}