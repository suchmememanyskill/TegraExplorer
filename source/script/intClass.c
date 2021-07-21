#include "intClass.h"
#include "StringClass.h"
#include "compat.h"
#include <malloc.h>
#include <string.h>
#include <utils/sprintf.h>

IntClass_t createIntClass(s64 in) {
	IntClass_t a = { in };
	return a;
}

Variable_t newIntVariable(s64 x) {
	// Integers are always read-only
	Variable_t var = { .variableType = IntClass, .readOnly = 1, .integer = createIntClass(x) };
	return var;
}

ClassFunction(printIntVariable) {
	IntClass_t* a = &caller->integer;
	gfx_printf("%d", (int)a->value);
	return &emptyClass;
}

ClassFunction(intToStr) {
	char buff[64] = {0};
	s_printf(buff, "%d", getIntValue(caller));
	return newStringVariablePtr(CpyStr(buff), 0, 1);
}

#define IntOpFunction(name, op) ClassFunction(name) { s64 i1 = getIntValue(caller); s64 i2 = getIntValue(*args); return newIntVariablePtr((i1 op i2)); }

IntOpFunction(addInt, +)
IntOpFunction(minusInt, -)
IntOpFunction(multInt, *)
IntOpFunction(divInt, /)
IntOpFunction(modInt, %)
IntOpFunction(smallerInt, <)
IntOpFunction(biggerInt, >)
IntOpFunction(smallerEqInt, <=)
IntOpFunction(biggerEqInt, >=)
IntOpFunction(eqInt, ==)
IntOpFunction(notEqInt, !=)
IntOpFunction(logicAndInt, &&)
IntOpFunction(logicOrInt, ||)
IntOpFunction(andInt, &)
IntOpFunction(orInt, |)
IntOpFunction(bitshiftLeftInt, <<)
IntOpFunction(bitshiftRightInt, >>)

ClassFunction(notInt) {
	return newIntVariablePtr(!(getIntValue(caller)));
}

u8 oneVarArgInt[] = { VARARGCOUNT };
u8 oneIntArgInt[] = { IntClass };

#define IntOpFunctionEntry(opName, functionName) {opName, functionName, 1, oneIntArgInt}

ClassFunctionTableEntry_t intFunctions[] = {
	{"print", printIntVariable, 0, 0},
	{"not", notInt, 0, 0},
	{"str", intToStr, 0, 0},
	IntOpFunctionEntry("+", addInt),
	IntOpFunctionEntry("-", minusInt),
	IntOpFunctionEntry("*", multInt),
	IntOpFunctionEntry("/", divInt),
	IntOpFunctionEntry("%", modInt),
	IntOpFunctionEntry("<", smallerInt),
	IntOpFunctionEntry(">", biggerInt),
	IntOpFunctionEntry("<=", smallerEqInt),
	IntOpFunctionEntry(">=", biggerEqInt),
	IntOpFunctionEntry("==", eqInt),
	IntOpFunctionEntry("!=", notEqInt),
	IntOpFunctionEntry("&&", logicAndInt),
	IntOpFunctionEntry("||", logicOrInt),
	IntOpFunctionEntry("&", andInt),
	IntOpFunctionEntry("|", orInt),
	IntOpFunctionEntry("<<", bitshiftLeftInt),
	IntOpFunctionEntry(">>", bitshiftRightInt),
};

Variable_t getIntegerMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, intFunctions, ARRAY_SIZE(intFunctions));
}