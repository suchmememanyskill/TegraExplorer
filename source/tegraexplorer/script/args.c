#include "args.h"
#include "parser.h"
#include "scriptCtx.h"
#include "lexer.h"
#include "functions.h"
#include "list.h"
#include <string.h>
#include "../../mem/heap.h"
#include "../utils/utils.h"

// do not include first openToken!
int distanceBetweenTokens(lexarToken_t* tokens, u32 len, int openToken, int closeToken) {
    int i = 0;
    int layer = 0;
    for (; i < len; i++) {
        if (tokens[i].token == openToken)
            layer++;
        else if (tokens[i].token == closeToken) {
            if (layer == 0)
                return i;
            layer--;
        }
    }

    return -1;
}

dictValue_t getVarFromToken(scriptCtx_t* ctx, lexarToken_t* tokens, int* index, u32 maxLen) {
    dictValue_t val = { 0 };
    val.free = 0;
    int i = *index;
    u8 not = 0;
    
    if (tokens[i].token == Not) {
        not = 1;
        i++;
    }

    if (tokens[i].token == Not)
        return ErrDictValue(ERRDOUBLENOT);

    if (tokens[i].token == Variable) {
        char* funcName = tokens[i].text;

        if (tokens[i + 1].token == LBracket) {
            i += 2;

            int argCount = distanceBetweenTokens(&tokens[i], maxLen - 2, LBracket, RBracket);
            if (argCount < 0)
                return ErrDictValue(ERRSYNTAX);

            ctx->args_loc.tokens = &tokens[i];
            ctx->args_loc.stored = argCount;
            val = executeFunction(ctx, funcName);
            
            i += argCount;
        }
        else if (tokens[i + 1].token == LSBracket) {
            dictValue_t* var = varVectorFind(&ctx->vars, tokens[i].text);
            i += 2;

            if (var == NULL)
                return ErrDictValue(ERRNOVAR);

            int argCount = distanceBetweenTokens(&tokens[i], maxLen - 2, LSBracket, RSBracket);
            if (argCount < 0)
                return ErrDictValue(ERRSYNTAX);

            dictValue_t index = solveEquation(ctx, &tokens[i], argCount);
            i += argCount;

            if (index.varType != IntType)
                return ErrDictValue(ERRINVALIDTYPE);

            if (var->arrayLen <= index.integer || index.integer < 0)
                return ErrDictValue(ERRSYNTAX);

            switch (var->varType) {
                case StringArrayType:
                    val = StrDictValue(var->stringArray[index.integer]);
                    break;
                case IntArrayType:
                    val = IntDictValue(var->integerArray[index.integer]);
                    break;
                case ByteArrayType:
                    val = IntDictValue(var->byteArray[index.integer]);
                    break;
                default:
                    return ErrDictValue(ERRINVALIDTYPE);
            } 
        }
        else {
            dictValue_t* var = varVectorFind(&ctx->vars, tokens[i].text);

            if (var != NULL) {
                if (var->type == IntType) {
                    val = IntDictValue(var->integer);
                }
                else if (var->type == StringType){
                    val = StrDictValue(util_cpyStr(var->string));
                    val.free = 1;
                }
                else {
                    val = *var;
                    val.free = 0;
                }
            }
            else
                return ErrDictValue(ERRNOVAR);
        }
    }
    else if (tokens[i].token == LSBracket) {
        i++;

        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 1, LSBracket, RSBracket);
        if (argCount <= 0)
            return ErrDictValue(-5);

        varVector_t arrayVars = extractVars(ctx, &tokens[i], argCount);
        int type = arrayVars.vars[0].value.varType;
        if (!(type == StringType || type == IntType))
            return ErrDictValue(ERRINVALIDTYPE);

        val.type = (type + 2) | FREEINDICT;
        val.arrayLen = arrayVars.stored;
        val.integerArray = calloc(val.arrayLen, (type == IntType) ? sizeof(int) : sizeof(char*));
        val.free = (type == StringType) ? 1 : 0;

        for (int i = 0; i < arrayVars.stored; i++) {
            if (arrayVars.vars[i].value.varType != type)
                return ErrDictValue(ERRINVALIDTYPE); // Free-ing issue!!

            if (type == StringType) {
                val.stringArray[i] = util_cpyStr(arrayVars.vars[i].value.string);
            }
            else {
                val.integerArray[i] = arrayVars.vars[i].value.integer;
            }
        }

        i += argCount;
        varVectorFree(&arrayVars);
    }
    else if (tokens[i].token == LBracket) {
        i += 1;
        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 1, LBracket, RBracket);
        if (argCount < 0)
            return ErrDictValue(ERRSYNTAX);

        val = solveEquation(ctx, &tokens[i], argCount);
        i += argCount;
    }
    else if (tokens[i].token == IntLit) {
        val = IntDictValue(tokens[i].val);
    }
    else if (tokens[i].token == StrLit) {
        val = StrDictValue(tokens[i].text); // Do we need to copy the string here? if we set var.free = 0, it shouldn't matter
        val.free = 0;
    }
    else {
        // ERR
        return ErrDictValue(ERRNOVAR);
    }

    if (not) {
        if (val.type == IntType)
            val.integer = !val.integer;
        else
            return ErrDictValue(ERRINVALIDTYPE);
    }

    *index = i;
    return val;
}

int matchTypes(dictValue_t d1, dictValue_t d2, int type) {
    return (d1.varType == d2.varType && d1.varType == type);
}

#define ELIFT(token) else if (localOpToken == token)

dictValue_t solveEquation(scriptCtx_t *ctx, lexarToken_t* tokens, u32 len) {
    dictValue_t res = { 0 };
    u16 lastToken = 0;
        
    for (int i = 0; i < len; i++) {
        if (tokens[i].token == Variable || tokens[i].token == StrLit || tokens[i].token == IntLit || tokens[i].token == Not || tokens[i].token == LBracket || tokens[i].token == LSBracket){
            dictValue_t val = getVarFromToken(ctx, tokens, &i, len - i);

            if (val.type == ErrType)
                return val;

            if (lastToken) {
                u16 localOpToken = lastToken; // do we need local op token?
                lastToken = 0;

                if (matchTypes(res, val, IntType)) {
                    if (localOpToken == Plus)
                        res.integer += val.integer;
                    ELIFT(Minus)
                        res.integer -= val.integer;
                    ELIFT(Multiply)
                        res.integer *= val.integer;
                    ELIFT(Division)
                        res.integer /= val.integer;
                    ELIFT(Mod)
                        res.integer %= val.integer;
                    ELIFT(Smaller)
                        res.integer = res.integer < val.integer;
                    ELIFT(SmallerEqual)
                        res.integer = res.integer <= val.integer;
                    ELIFT(Bigger)
                        res.integer = res.integer > val.integer;
                    ELIFT(BiggerEqual)
                        res.integer = res.integer >= val.integer;
                    ELIFT(EqualEqual)
                        res.integer = res.integer == val.integer;
                    ELIFT(NotEqual)
                        res.integer = res.integer != val.integer;
                    ELIFT(LogicAND) {
                        res.integer = res.integer && val.integer;
                        if (!res.integer)
                            break;
                    }
                    ELIFT(LogicOR) {
                        res.integer = res.integer || val.integer;
                        if (res.integer)
                            break;
                    }
                    ELIFT(AND)
                        res.integer = res.integer & val.integer;
                    ELIFT(OR)
                        res.integer = res.integer | val.integer;
                    else
                        return ErrDictValue(ERRBADOPERATOR);
                }
                else if (matchTypes(res, val, StringType)) {
                    if (localOpToken == Plus) {
                        char* buff = calloc(strlen(res.string) + strlen(val.string) + 1, 1);
                        strcpy(buff, res.string);
                        strcat(buff, val.string);
                        if (res.free)
                            free(res.string);
                        if (val.free)
                            free(val.string);
                        res.string = buff;
                    }
                    ELIFT(EqualEqual) {
                        res.type = IntType;
                        int compRes = !strcmp(res.string, val.string);
                        if (res.free)
                            free(res.string);
                        if (val.free)
                            free(val.string);
                        res.integer = compRes;
                        res.free = 0;
                    }
                    ELIFT(Minus) {
                        if (!strcmp(res.string + strlen(res.string) - strlen(val.string), val.string)) {
                            *(res.string + strlen(res.string) - strlen(val.string)) = 0;
                        }

                        free(val.string);
                    }
                    else 
                        return ErrDictValue(ERRBADOPERATOR);
                }
                else
                    return ErrDictValue(ERRBADOPERATOR);
            }
            else {
                res = val;
            }
        }
        else if (tokens[i].token >= Plus && tokens[i].token <= OR) {
            lastToken = tokens[i].token;
        }
    }

    return res;
}