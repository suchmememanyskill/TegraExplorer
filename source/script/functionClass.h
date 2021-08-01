#pragma once
#include "model.h"

Function_t createEmptyFunction();
Function_t* createFunctionPtrFromFunction(Function_t in);
Variable_t newFunctionVariable(FunctionClass_t func);
FunctionClass_t createFunctionClass(Function_t in, ClassFunctionTableEntry_t* builtIn);
int countTokens(Function_t* func, u8 toCount);