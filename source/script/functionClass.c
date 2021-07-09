#include "functionClass.h"
#include "compat.h"
#include "model.h"
#include <malloc.h>

Function_t* getFunctionValue(Variable_t* var) {
	if (var->variableType != FunctionClass)
		return NULL;

	FunctionClass_t* a = &var->function;

	if (a->builtIn)
		return NULL;

	return &a->function;
}

Function_t createEmptyFunction() {
	Function_t a = { 0 };
	a.operations = newVec(sizeof(Operator_t), 0);
	return a;
}

// Will NOT copy the Function, the pointer is taken as-is. Set as NULL to make it builtin
FunctionClass_t createFunctionClass(Function_t in, ClassFunctionTableEntry_t *builtIn) {
	FunctionClass_t a = { 0 };
	if (!builtIn) {
		a.function = in;
	}
	else {
		a.builtIn = 1;
		a.builtInPtr = builtIn;
		a.len = 1;
	}

	return a;
}

FunctionClass_t* creteFunctionClassPtr(Function_t in, ClassFunctionTableEntry_t* builtIn) {
	FunctionClass_t* a = malloc(sizeof(FunctionClass_t));
	*a = createFunctionClass(in, builtIn);
	return a;
}

Function_t* createFunctionPtrFromFunction(Function_t in) {
	Function_t* f = malloc(sizeof(Function_t));
	*f = in;
	return f;
}

// Functions are always readonly
Variable_t newFunctionVariable(FunctionClass_t func) {
	Variable_t var = { .variableType = FunctionClass, .readOnly = 1, .function = func };
	return var;
}

int countTokens(Function_t *func, u8 toCount) {
	Operator_t* ops = func->operations.data;
	int count = 0;
	for (int i = 0; i < func->operations.count; i++) {
		if (ops[i].token == toCount) {
			count++;
		}
	}

	return count;
}