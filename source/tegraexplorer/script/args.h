#pragma once
#include "types.h"

char* createStrOutTextHolder(textHolder_t t);
dictValue_t solveEquation(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len);
int distanceBetweenTokens(lexarToken_t* tokens, u32 len, int openToken, int closeToken);