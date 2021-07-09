#pragma once
#include "model.h"
#include "genericClass.h"
#include "compat.h"

Variable_t createUnsolvedArrayVariable(Function_t* f);
Variable_t* solveArray(Variable_t* unsolvedArray);
Variable_t getUnsolvedArrayMember(Variable_t* var, char* memberName);