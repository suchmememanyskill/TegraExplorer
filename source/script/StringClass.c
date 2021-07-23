#include "StringClass.h"
#include "compat.h"
#include "intClass.h"
#include "scriptError.h"
#include <string.h>

char* getStringValue(Variable_t* var) {
	if (var->variableType != StringClass)
		return NULL;

	return var->string.value;
}

// Will NOT copy the string, the pointer is taken as-is
StringClass_t createStringClass(char* in, u8 free) {
	StringClass_t a = { 0 };
	a.free = free;
	a.value = in;
	return a;
}

Variable_t newStringVariable(char *x, u8 readOnly, u8 freeOnExit) {
	Variable_t var = { .variableType = StringClass, .readOnly = readOnly, .string = createStringClass(x, freeOnExit) };
	return var;
}

ClassFunction(printStringVariable) {
	if (caller->variableType == StringClass) {
		StringClass_t* a = &caller->string;
		gfx_printf("%s", a->value);
	}
	return &emptyClass;
}

ClassFunction(addStringVariables) {
	char* s1 = getStringValue(caller);
	char* s2 = getStringValue(*args);

	char* n = malloc(strlen(s1) + strlen(s2) + 1);
	strcpy(n, s1);
	strcat(n, s2);

	return newStringVariablePtr(n, 0, 1);
}

ClassFunction(getStringLength) {
	char* s1 = getStringValue(caller);
	return newIntVariablePtr(strlen(s1));
}

ClassFunction(stringBytes) {
	Variable_t v = { 0 };
	v.variableType = ByteArrayClass;
	u32 len = strlen(caller->string.value);
	v.solvedArray.vector = newVec(1, len);
	v.solvedArray.vector.count = len;
	memcpy(v.solvedArray.vector.data, caller->string.value, len);
	return copyVariableToPtr(v);
}

ClassFunction(stringIndexGet) {
	u32 len = strlen(caller->string.value);
	u32 idx = args[0]->integer.value;
	if (len < idx || idx < 0) {
		SCRIPT_FATAL_ERR("Index of string out of range");
	}

	char* a = calloc(1,2);
	a[0] = caller->string.value[idx];
	return newStringVariablePtr(a, 0, 0);
}

ClassFunction(stringMinusInt){
	u32 baseStrLen = strlen(caller->string.value);
	if (baseStrLen < args[0]->integer.value){
		SCRIPT_FATAL_ERR("Index of string out of range");
	}
	char* newStr = calloc(baseStrLen - args[0]->integer.value + 1, 1);
	memcpy(newStr, caller->string.value, baseStrLen - args[0]->integer.value);
	return newStringVariablePtr(newStr, 0, 1);
}

ClassFunction(stringEq){
	return newIntVariablePtr(!strcmp(caller->string.value, args[0]->string.value));
}

ClassFunction(stringInEq){
	return newIntVariablePtr(strcmp(caller->string.value, args[0]->string.value));
}

u8 strOneIntArg[] = { IntClass };
u8 oneStringArg[] = { StringClass };

ClassFunctionTableEntry_t stringFunctions[] = {
	{"print", printStringVariable, 0, 0},
	{"+", addStringVariables, 1, oneStringArg },
	{"len", getStringLength, 0, 0},
	{"bytes", stringBytes, 0, 0},
	{"get", stringIndexGet, 1, strOneIntArg},
	{"-", stringMinusInt, 1, strOneIntArg},
	{"==", stringEq, 1, oneStringArg},
	{"!=", stringInEq, 1, oneStringArg},
};

Variable_t getStringMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, stringFunctions, ARRAY_SIZE(stringFunctions));
}