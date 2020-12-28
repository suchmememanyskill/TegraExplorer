#pragma once
#include "types.h"

char* utils_copyStringSize(const char* in, int size);
Variable_t solveEquation(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len, u8 shouldFree);
int distanceBetweenTokens(lexarToken_t* tokens, u32 len, int openToken, int closeToken);
Vector_t extractVars(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len);