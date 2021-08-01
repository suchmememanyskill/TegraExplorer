#pragma once
#include "model.h"
#include "genericClass.h"

StringClass_t createStringClass(char* in, u8 free);
char* getStringValue(Variable_t* var);

Variable_t newStringVariable(char *x, u8 readOnly, u8 freeOnExit);
#define newStringVariablePtr(x, readOnly, freeOnExit) copyVariableToPtr(newStringVariable(x, readOnly, freeOnExit))
Variable_t getStringMember(Variable_t* var, char* memberName);