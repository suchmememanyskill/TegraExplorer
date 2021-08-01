#pragma once
#include "model.h"
#include "genericClass.h"
#include "compat.h"

Variable_t getDictMember(Variable_t* var, char* memberName);
void addVariableToDict(Variable_t *dict, char* name, Variable_t *add);
void addIntToDict(Variable_t *dict, char* name, s64 integer);