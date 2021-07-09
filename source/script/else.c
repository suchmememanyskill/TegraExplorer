#include "else.h"

ClassFunction(scriptElse) {
	if (!caller->integer.value) {
		Variable_t* res = genericCallDirect(args[0], NULL, 0);
		if (res == NULL)
			return NULL;

		removePendingReference(res);
	}
	return &emptyClass;
}

u8 elseOneFunction[] = { FunctionClass };

ClassFunctionTableEntry_t elseFunctions[] = {
	{"else", scriptElse, 1, elseOneFunction},
};

Variable_t getElseMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, elseFunctions, ARRAY_SIZE(elseFunctions));
}