#pragma once
#include "model.h"

typedef struct {
	struct {
		u8 isTopLevel : 1;
		u8 hasBeenNoticed : 1;
		u8 hasVarName : 1;
	};
	Variable_t* setVar;
	char* varName;
	Function_t* idxVar;
} Callback_SetVar_t;

Variable_t* eval(Operator_t* ops, u32 len, u8 ret);
void setStaticVars(Vector_t* vec);
void initRuntimeVars();
void exitRuntimeVars();
void runtimeVariableEdit(Callback_SetVar_t* set, Variable_t* curRes);