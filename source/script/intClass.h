#pragma once
#include "model.h"
#include "genericClass.h"



#define getIntValue(var) (var)->integer.value

IntClass_t createIntClass(s64 in);

Variable_t newIntVariable(s64 x);
#define newIntVariablePtr(x) copyVariableToPtr(newIntVariable(x))
Variable_t getIntegerMember(Variable_t* var, char* memberName);