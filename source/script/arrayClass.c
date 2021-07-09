#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "intClass.h"
#include "arrayClass.h"
#include "garbageCollector.h"
#include "eval.h"
#include "scriptError.h"
#include "StringClass.h"
#include <string.h>

u8 anotherOneIntArg[] = { IntClass };
u8 oneStringoneFunction[] = { StringClass, FunctionClass };
u8 oneIntOneAny[] = { IntClass, VARARGCOUNT };
u8 anotherAnotherOneVarArg[] = { VARARGCOUNT };
u8 oneByteArrayClass[] = {ByteArrayClass};
u8 oneIntArrayClass[] = {IntArrayClass};

Variable_t arrayClassGetIdx(Variable_t *caller, s64 idx) {
	if (caller->variableType == IntArrayClass) {
		s64* arr = caller->solvedArray.vector.data;
		return newIntVariable(arr[idx]);
	}
	else if (caller->variableType == StringArrayClass) {
		char** arr = caller->solvedArray.vector.data;
		Variable_t v = newStringVariable(arr[idx], 1, 0);
		v.readOnly = 1;
		v.reference = 1;
		return v;
	}
	else if (caller->variableType == ByteArrayClass) {
		u8* arr = caller->solvedArray.vector.data;
		return newIntVariable(arr[idx]);
	}

	return (Variable_t) { 0 };
}

ClassFunction(getArrayIdx) {
	s64 getVal = (*args)->integer.value;
	// Out of bounds
	if (getVal < 0 || getVal >= caller->solvedArray.vector.count) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)getVal, (int)caller->solvedArray.vector.count);
	}

	Variable_t a = arrayClassGetIdx(caller, getVal);
	if (a.variableType == None)
		return NULL;
	return copyVariableToPtr(a);
}

ClassFunction(getArrayLen) {
	return newIntVariablePtr(caller->solvedArray.vector.count);
}

ClassFunction(createRefSkip) {
	s64 skipAmount = getIntValue(*args);
	if (caller->solvedArray.vector.count < skipAmount || skipAmount <= 0) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)skipAmount, (int)caller->solvedArray.vector.count);
	}
		
	Variable_t refSkip = { .variableType = SolvedArrayReferenceClass };
	refSkip.solvedArray.arrayClassReference = caller;
	refSkip.solvedArray.offset = skipAmount;
	refSkip.solvedArray.len = caller->solvedArray.vector.count - skipAmount;
	addPendingReference(caller);
	return copyVariableToPtr(refSkip);
}


ClassFunction(takeArray) {
	s64 skipAmount = getIntValue(*args);
	if (caller->solvedArray.vector.count < skipAmount || skipAmount <= 0) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)skipAmount, (int)caller->solvedArray.vector.count);
	}

	Variable_t refSkip = { .variableType = SolvedArrayReferenceClass };
	refSkip.solvedArray.arrayClassReference = caller;
	refSkip.solvedArray.len = skipAmount;
	addPendingReference(caller);
	return copyVariableToPtr(refSkip);
}


ClassFunction(arrayForEach) {
	Vector_t* v = &caller->solvedArray.vector;

	Callback_SetVar_t setVar = { .isTopLevel = 1, .varName = (*args)->string.value };
	Variable_t* iter = NULL;
	iter = copyVariableToPtr(newIntVariable(0));

	runtimeVariableEdit(&setVar, iter);

	for (int i = 0; i < v->count; i++) {
		*iter = arrayClassGetIdx(caller, i);

		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL)
			return NULL;
	}

	return &emptyClass;
}

ClassFunction(arrayCopy) {
	Vector_t* v = &caller->solvedArray.vector;
	Vector_t copiedArray = vecCopy(v);
	Variable_t var = { .variableType = caller->variableType, .solvedArray.vector = copiedArray };
	return copyVariableToPtr(var);
}

ClassFunction(arraySet) {
	s64 idx = getIntValue(*args);
	Vector_t* v = &caller->solvedArray.vector;
	if (v->count < idx || idx <= 0) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)idx, (int)caller->solvedArray.vector.count);
	}

	if (caller->readOnly) {
		SCRIPT_FATAL_ERR("Array is read-only");
	}

	if (caller->variableType == IntArrayClass) {
		if (args[1]->variableType != IntClass) {
			return NULL; // TODO: add proper error handling
		}

		s64* a = v->data;
		a[idx] = getIntValue(args[1]);
	}
	else if (caller->variableType == StringArrayClass) {
		if (args[1]->variableType != StringClass) {
			return NULL; // TODO: add proper error handling
		}

		char** a = v->data;
		FREE(a[idx]);
		a[idx] = CpyStr(args[1]->string.value);
	}
	else if (caller->variableType == ByteArrayClass) {
		if (args[1]->variableType != IntClass) {
			return NULL; // TODO: add proper error handling
		}

		u8* a = v->data;
		a[idx] = (u8)(getIntValue(args[1]) & 0xFF);
	}

	return &emptyClass;
}

ClassFunction(arrayAdd) {
	Variable_t* arg = *args;

	if (caller->variableType == IntArrayClass) {
		if (arg->variableType != IntClass) {
			return NULL; // TODO: add proper error handling
		}

		vecAdd(&caller->solvedArray.vector, arg->integer.value);
	}
	else if (caller->variableType == StringArrayClass) {
		if (arg->variableType != StringClass) {
			return NULL; // TODO: add proper error handling
		}

		char* str = CpyStr(arg->string.value);
		vecAdd(&caller->solvedArray.vector, str);
	}
	else if (caller->variableType == ByteArrayClass) {
		if (arg->variableType != IntClass) {
			return NULL; // TODO: add proper error handling
		}

		u8 val = (u8)(arg->integer.value & 0xFF);
		vecAdd(&caller->solvedArray.vector, val);
	}

	return &emptyClass;
}

ClassFunction(arrayContains) {
	Vector_t* v = &caller->solvedArray.vector;
	Variable_t* arg = *args;

	for (int i = 0; i < v->count; i++) {
		Variable_t iter = arrayClassGetIdx(caller, i);
	
		if (caller->variableType == StringArrayClass) {
			if (!strcmp(arg->string.value, iter.string.value))
				return newIntVariablePtr(1);
		}
		else {
			if (arg->integer.value == iter.integer.value)
				return newIntVariablePtr(1);
		}
	}

	return newIntVariablePtr(0);
}

ClassFunction(arrayMinus) {
	s64 count = getIntValue(*args);
	Vector_t* v = &caller->solvedArray.vector;
	if (v->count < count || count <= 0) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)count, (int)caller->solvedArray.vector.count);
	}

	if (caller->variableType == StringArrayClass) {
		char** arr = v->data;
		for (int i = v->count - count; i < count; i++) {
			FREE(arr[i]);
		}
	}

	v->count -= count;
	return &emptyClass;
}

ClassFunction(bytesToStr) {
	if (caller->variableType != ByteArrayClass) {
		SCRIPT_FATAL_ERR("Nedd a bytearray to convert to str");
	}

	char* buff = malloc(caller->solvedArray.vector.count + 1);
	memcpy(buff, caller->solvedArray.vector.data, caller->solvedArray.vector.count);
	buff[caller->solvedArray.vector.count] = '\0';
	return newStringVariablePtr(buff, 0, 0);
}

ClassFunction(eqArray){
	Variable_t *arg = (*args);
	if (caller->solvedArray.vector.count != arg->solvedArray.vector.count || arg->variableType != caller->variableType){
		return newIntVariablePtr(0);
	}	

	s64 res = memcmp(caller->solvedArray.vector.data, arg->solvedArray.vector.data, caller->solvedArray.vector.count * caller->solvedArray.vector.elemSz);
	return newIntVariablePtr(!res);
}

ClassFunctionTableEntry_t arrayFunctions[] = {
	{"get", getArrayIdx, 1, anotherOneIntArg },
	{"len", getArrayLen, 0, 0},
	{"skip", createRefSkip, 1, anotherOneIntArg},
	{"foreach", arrayForEach, 2, oneStringoneFunction},
	{"copy", arrayCopy, 0, 0},
	{"set", arraySet, 2, oneIntOneAny},
	{"+", arrayAdd, 1, anotherAnotherOneVarArg},
	{"-", arrayMinus, 1, anotherOneIntArg},
	{"take", takeArray, 1, anotherOneIntArg},
	{"contains", arrayContains, 1, anotherAnotherOneVarArg},
	{"bytestostr", bytesToStr, 0, 0},
	{"==", eqArray, 1, oneByteArrayClass},
	{"==", eqArray, 1, oneIntArrayClass},
};

Variable_t getArrayMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, arrayFunctions, ARRAY_SIZE(arrayFunctions));
}