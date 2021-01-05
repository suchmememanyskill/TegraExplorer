#pragma once
#include "variables.h"

Variable_t executeFunction(scriptCtx_t* ctx, char* func_name, lexarToken_t* start, u32 len);