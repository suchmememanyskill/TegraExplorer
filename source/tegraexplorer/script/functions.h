#pragma once
#include "types.h"

#define BIT(n) (1U << n)

#define VARARGS(x) {x, 1, 0}
#define OPERATORARGS(x) {x, 0, 1}

dictValue_t executeFunction(scriptCtx_t* ctx, char* func_name);
varVector_t extractVars(scriptCtx_t* ctx, lexarToken_t* tokens, u32 len);