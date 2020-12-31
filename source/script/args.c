#include "args.h"
#include "types.h"
#include "functions.h"
#include "variables.h"
#include <string.h>
#include <mem/heap.h>
#include "../utils/utils.h"

char* utils_copyStringSize(const char* in, int size) {
    if (size > strlen(in) || size < 0)
        size = strlen(in);

    char* out = calloc(size + 1, 1);
    //strncpy(out, in, size);
    if (size)
        memcpy(out, in, size);
    return out;
}

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

Vector_t extractVars(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len) {
    Vector_t args = newVec(sizeof(Variable_t), 5);
    int lastLoc = 0;
    for (int i = 0; i < len; i++) {
        if (tokens[i].token == LSBracket) {
            int distance = distanceBetweenTokens(&tokens[i + 1], len - i - 1, LSBracket, RSBracket);
            i += distance + 1;
        }
        if (tokens[i].token == Seperator) {
            Variable_t res = solveEquation(ctx, &tokens[lastLoc], i - lastLoc, 0);
            lastLoc = i + 1;

            vecAddElement(&args, res);
        }

        if (i + 1 >= len) {
            Variable_t res = solveEquation(ctx, &tokens[lastLoc], i + 1 - lastLoc, 0);
            vecAddElement(&args, res);
        }
    }

    return args;
}

#define ErrValue(err) (Variable_t) {.varType = ErrType, .integerType = err}
#define IntValue(i) (Variable_t) {.varType = IntType, .integerType = i}
#define StrValue(s) (Variable_t) {.varType = StringType, .stringType = s} 

#define ELIFTX(x) else if (tokens[i].token == x)

Variable_t getVarFromToken(scriptCtx_t* ctx, lexarToken_t* tokens, int* index, u32 maxLen) {
    Variable_t val = { 0 };
    int i = *index;

    if (tokens[i].token == Variable) {
        Variable_t* var = dictVectorFind(&ctx->varDict, tokens[i].text);
        if (var != NULL) {
            val = *var;
            val.free = 0;
        }
        else {
            val = ErrValue(ERRNOVAR);
        }
    }
    ELIFTX(IntLit) {
        val = IntValue(tokens[i].val);
    }
    ELIFTX(StrLit) {
        val = StrValue(tokens[i].text);
        val.free = 0;
    }
    ELIFTX(ArrayVariable) {
        Variable_t* var = dictVectorFind(&ctx->varDict, tokens[i].text);
        i += 2;

        if (var == NULL)
            return ErrValue(ERRNOVAR);

        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 2, LSBracket, RSBracket);
        if (argCount < 0)
            return ErrValue(ERRSYNTAX);

        Variable_t index = solveEquation(ctx, &tokens[i], argCount, 0);
        i += argCount;

        if (index.varType != IntType)
            return ErrValue(ERRINVALIDTYPE);

        if (var->vectorType.count <= index.integerType || index.integerType < 0)
            return ErrValue(ERRSYNTAX);

        switch (var->varType) {
            case StringArrayType:
                val = StrValue((vecGetArray(char**, var->vectorType))[index.integerType]);
                break;
            case IntArrayType:
                val = IntValue((vecGetArray(int*, var->vectorType))[index.integerType]);
                break;
            case ByteArrayType:
                val = IntValue((vecGetArray(u8*, var->vectorType))[index.integerType]);
                break;
            default:
                return ErrValue(ERRINVALIDTYPE);
        }
    }
    ELIFTX(Function) {
        i += 2;

        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 2, LBracket, RBracket);
        if (argCount < 0)
            return ErrValue(ERRSYNTAX);

        val = executeFunction(ctx, tokens[i - 2].text, &tokens[i], argCount);

        //val = IntValue(1);
        i += argCount;
    }
    ELIFTX(LSBracket) {
        i++;

        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 1, LSBracket, RSBracket);
        if (argCount <= 0)
            return ErrValue(ERRSYNTAX);

        // ArrayVars should be a Vector_t containing Variable_t's. Not implemented yet!
        Vector_t arrayVars = extractVars(ctx, &tokens[i], argCount);
        Variable_t* variables = vecGetArray(Variable_t*, arrayVars);
        int type = variables[0].varType;
        if (!(type == StringType || type == IntType))
            return ErrValue(ERRINVALIDTYPE);

        val.varType = (type + 2);
        val.free = 1;
        val.vectorType = newVec((type == IntType) ? sizeof(int) : sizeof(char*), arrayVars.count);

        for (int i = 0; i < arrayVars.count; i++) {
            if (variables[i].varType != type)
                return ErrValue(ERRINVALIDTYPE); // Free-ing issue!!

            if (type == StringType) {
                char* temp = CpyStr(variables[i].stringType);
                vecAddElement(&val.vectorType, temp);
            }
            else {
                vecAddElement(&val.vectorType, variables[i].integerType);
            }
        }

        i += argCount;
        freeVariableVector(&arrayVars);
    }
    ELIFTX(LBracket) {
        i++;
        int argCount = distanceBetweenTokens(&tokens[i], maxLen - 1, LBracket, RBracket);
        if (argCount < 0)
            return ErrValue(ERRSYNTAX);

        val = solveEquation(ctx, &tokens[i], argCount, 0);
        i += argCount;
    }
    else {
        // ERR
        return ErrValue(ERRSYNTAX);
    }

    *index = i;
    return val;
}

int matchTypes(Variable_t d1, Variable_t d2, int type) {
    return (d1.varType == d2.varType && d1.varType == type);
}

#define ELIFT(token) else if (localOpToken == token)

Variable_t solveEquation(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len, u8 shouldFree) {
    Variable_t res = { 0 };
    u8 lastToken = 0;
    u8 invertValue = 0;
    lexarToken_t* varToken = NULL;

    for (int i = 0; i < len; i++) {
        if (tokens[i].token == Not) 
            invertValue = !invertValue;

        else if (tokens[i].token == ArrayVariableAssignment || tokens[i].token == VariableAssignment) {
            varToken = &tokens[i];
            if (tokens[i].token == ArrayVariableAssignment) {
                int distance = distanceBetweenTokens(&tokens[i] + 2, len, LSBracket, RSBracket);
                i += distance + 2;
            }
        }
            
        else if (tokens[i].token >= Variable && tokens[i].token <= LSBracket) {
            Variable_t val = getVarFromToken(ctx, tokens, &i, len - i);

            if (val.varType == ErrType)
                return val;

            if (val.varType == IntType && invertValue) {
                val.integerType = !val.integerType;
            }
            invertValue = 0;

            if (lastToken) {
                u16 localOpToken = lastToken; // do we need local op token?
                lastToken = 0;

                if (matchTypes(res, val, IntType)) {
                    if (localOpToken == Plus)
                        res.integerType += val.integerType;
                    ELIFT(Minus)
                        res.integerType -= val.integerType;
                    ELIFT(Multiply)
                        res.integerType *= val.integerType;
                    ELIFT(Division) {
                        if (!val.integerType)
                            res = ErrValue(ERRDIVBYZERO);
                        else
                            res.integerType = res.integerType / val.integerType;
                    }
                    ELIFT(Mod)
                        res.integerType %= val.integerType;
                    ELIFT(Smaller)
                        res.integerType = res.integerType < val.integerType;
                    ELIFT(SmallerEqual)
                        res.integerType = res.integerType <= val.integerType;
                    ELIFT(Bigger)
                        res.integerType = res.integerType > val.integerType;
                    ELIFT(BiggerEqual)
                        res.integerType = res.integerType >= val.integerType;
                    ELIFT(EqualEqual)
                        res.integerType = res.integerType == val.integerType;
                    ELIFT(NotEqual)
                        res.integerType = res.integerType != val.integerType;
                    ELIFT(LogicAND) {
                        res.integerType = res.integerType && val.integerType;
                        if (!res.integerType)
                            break;
                    }
                    ELIFT(LogicOR) {
                        res.integerType = res.integerType || val.integerType;
                        if (res.integerType)
                            break;
                    }
                    ELIFT(AND)
                        res.integerType = res.integerType & val.integerType;
                    ELIFT(OR)
                        res.integerType = res.integerType | val.integerType;
                    else
                        return ErrValue(ERRBADOPERATOR);
                }
                else if (matchTypes(res, val, StringType)) {
                    if (localOpToken == Plus) {
                        char* buff = calloc(strlen(res.stringType) + strlen(val.stringType) + 1, 1);
                        strcpy(buff, res.stringType);
                        strcat(buff, val.stringType);

                        if (res.free) free(res.stringType); // we should replace these with variablefree
                        if (val.free) free(val.stringType);
                        res.stringType = buff;
                        res.free = 1;
                    }
                    ELIFT(EqualEqual) {
                        res.typeUnion = IntType;
                        int compRes = !strcmp(res.stringType, val.stringType);
                        if (res.free) free(res.stringType);
                        if (val.free) free(val.stringType);
                        res.integerType = compRes;
                        res.free = 0;
                    }
                    ELIFT(Minus) {
                        u32 lenRes = strlen(res.stringType);
                        u32 valRes = strlen(val.stringType);
                        if (!strcmp(res.stringType + lenRes - valRes, val.stringType)) {
                            char *temp = malloc(lenRes - valRes + 1);
                            memcpy(temp, res.stringType, lenRes - valRes);
                            temp[lenRes - valRes] = 0;
                            freeVariable(res);
                            res.free = 1;
                            res.stringType = temp;
                        }

                        freeVariable(val);
                    }
                    ELIFT(Division) {
                        int valLen = strlen(val.stringType);
                        if (!valLen) {
                            res = ErrValue(ERRSYNTAX);
                            continue;
                        }

                        char* start = res.stringType;
                        char* find = NULL;
                        Vector_t arr = newVec(sizeof(char**), 10);
                        char* temp;

                        while ((find = (strstr(start, val.stringType))) != NULL) {
                            temp = utils_copyStringSize(start, find - start);
                            vecAddElement(&arr, temp);

                            start = find + valLen;
                        }

                        temp = utils_copyStringSize(start, res.stringType + strlen(res.stringType) - start);
                        vecAddElement(&arr, temp);
                        if (res.free) free(res.stringType); // do we free here?
                        if (val.free) free(val.stringType);

                        res.varType = StringArrayType;
                        res.free = 1;
                        res.vectorType = arr;
                    }
                    else
                        return ErrValue(ERRBADOPERATOR);
                }
                else if ((res.varType == IntArrayType || res.varType == ByteArrayType) && val.varType == IntType) {
                    if (localOpToken == Plus) {
                        Vector_t newV = vecCopy(&res.vectorType);
                        freeVariable(res);
                        res.vectorType = newV;
                        res.free = 1;
                        if (res.varType == IntArrayType)
                            vecAddElement(&res.vectorType, val.integerType);
                        else {
                            u8 in = ((u8)val.integerType & 0xFF);
                            vecAddElement(&res.vectorType, in);
                        }
                    }
                }
                else if (res.varType == StringType && val.varType == IntType){
                    if (localOpToken == Minus){
                        u32 resLen = strlen(res.stringType);
                        if (resLen < val.integerType)
                            return ErrValue(ERRSYNTAX);
                        
                        char *temp = malloc(resLen - val.integerType + 1);
                        memcpy(temp, res.stringType, resLen - val.integerType);
                        temp[resLen - val.integerType] = 0;

                        freeVariable(res);
                        res.stringType = temp;
                        res.free = 1;
                    }
                    ELIFT(Selector){
                        u32 resLen = strlen(res.stringType);
                        if (resLen < val.integerType)
                            return ErrValue(ERRSYNTAX);
                        
                        char *temp = malloc(resLen - val.integerType + 1);
                        memcpy(temp, res.stringType + val.integerType, resLen - val.integerType);
                        temp[resLen - val.integerType] = 0;

                        freeVariable(res);
                        res.stringType = temp;
                        res.free = 1;
                    }
                    else
                        return ErrValue(ERRBADOPERATOR);
                }
                else
                    return ErrValue(ERRBADOPERATOR);
            }
            else {
                res = val;
            }
        }
        else if (tokens[i].token >= Plus && tokens[i].token <= Selector) {
            lastToken = tokens[i].token;
        }
    }

    if (varToken != NULL) {
        if (varToken->token == VariableAssignment) {
            dict_t newVar = newDict(CpyStr(varToken->text), res);
            dictVectorAdd(&ctx->varDict, newVar);
        }
        else {
            Variable_t* var = dictVectorFind(&ctx->varDict, varToken->text);
            if (var != NULL) {
                if (var->varType - 2 == res.varType || (var->varType == ByteArrayType && res.varType == IntType)) {
                    int distance = distanceBetweenTokens(varToken + 2, len, LSBracket, RSBracket);
                    if (distance < 0) {
                        // ERR
                        return ErrValue(ERRSYNTAX);
                    }
                    Variable_t index = solveEquation(ctx, varToken + 2, distance, 0);
                    if (index.varType != IntType) {
                        // ERR
                    }
                    else if (index.integerType < 0 || var->vectorType.count <= index.integerType) {
                        // ERR
                    }
                    else {
                        if (var->varType == IntArrayType) {
                            int* arr = vecGetArray(int*, var->vectorType);
                            arr[index.integerType] = res.integerType;
                        }
                        else if (var->varType == StringArrayType) {
                            char** arr = vecGetArray(char**, var->vectorType);
                            arr[index.integerType] = CpyStr(res.stringType);
                        }
                        else if (var->varType == ByteArrayType) {
                            u8* arr = vecGetArray(u8*, var->vectorType);
                            arr[index.integerType] = res.integerType & 0xFF;
                        }
                    }
                }
                else {
                    // ERR
                }
            }
            else {
                //ERR
            }
        }
    }
    else {
        if (shouldFree) // we should get rid of this ugly free. why not set .free to 0 when assigning it then free in parser.c?
            freeVariable(res);
    }

    return res;
}