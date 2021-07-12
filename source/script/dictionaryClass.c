#include "dictionaryClass.h"
#include <string.h>
#include "garbageCollector.h"

u8 dictOneStrOneAll[] = { StringClass, VARARGCOUNT };

Dict_t* getEntry(Vector_t *v, char* name) {
	vecForEach(Dict_t*, dict, v) {
		if (!strcmp(name, dict->name)) {
			return dict;
		}
	}
	return NULL;
}

ClassFunction(dictSet) {
	addPendingReference(args[1]);
	char* arg = CpyStr(args[0]->string.value);
	Dict_t* possibleEntry = getEntry(&caller->dictionary.vector, arg);
	if (possibleEntry == NULL) {
		Dict_t a = { .name = arg, .var = args[1] };
		vecAdd(&caller->dictionary.vector, a);
	}
	else {
		possibleEntry->var = args[1];
		free(arg);
	}
	return &emptyClass;
}

ClassFunctionTableEntry_t dictFunctions[] = {
	{"set", dictSet, 2, dictOneStrOneAll},
};

Variable_t getDictMember(Variable_t* var, char* memberName) {
	if (!strcmp(memberName, "set"))
		return getGenericFunctionMember(var, memberName, dictFunctions, ARRAY_SIZE(dictFunctions));

	vecForEach(Dict_t*, dict, (&var->dictionary.vector)) {
		if (!strcmp(dict->name, memberName)) {
			Variable_t a = { 0 };
			a.variableType = ReferenceType;
			a.referenceType = dict->var;
			addPendingReference(dict->var);
			return a;
		}
	}

	return (Variable_t) { 0 };
}