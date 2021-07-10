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
u8 doubleInt[] = {IntClass, IntClass};
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

int arrayClassAdd(Variable_t *caller, Variable_t *add){
	if (caller->variableType == IntArrayClass) {
		if (add->variableType != IntClass) {
			return 1;
		}

		vecAdd(&caller->solvedArray.vector, add->integer.value);
	}
	else if (caller->variableType == StringArrayClass) {
		if (add->variableType != StringClass) {
			return 1;
		}

		char* str = CpyStr(add->string.value);
		vecAdd(&caller->solvedArray.vector, str);
	}
	else if (caller->variableType == ByteArrayClass) {
		if (add->variableType != IntClass) {
			return 1;
		}

		u8 val = (u8)(add->integer.value & 0xFF);
		vecAdd(&caller->solvedArray.vector, val);
	}

	return 0;
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

ClassFunction(arraySlice) {
	s64 skipAmount = getIntValue(*args);
	s64 takeAmount = getIntValue(args[1]);

	if (caller->solvedArray.vector.count < (skipAmount + takeAmount)  || skipAmount <= 0 || takeAmount <= 0) {
		SCRIPT_FATAL_ERR("Slicing out of range of array with len %d", (int)caller->solvedArray.vector.count);
	}
		
	Variable_t refSkip = { .variableType = SolvedArrayReferenceClass };
	refSkip.solvedArray.arrayClassReference = caller;
	refSkip.solvedArray.offset = skipAmount;
	refSkip.solvedArray.len = takeAmount;
	addPendingReference(caller);
	return copyVariableToPtr(refSkip);
}

// TODO: arrayForEach does not like the new garbage collector
ClassFunction(arrayForEach) {
	Vector_t* v = &caller->solvedArray.vector;

	Callback_SetVar_t setVar = { .isTopLevel = 1, .varName = (*args)->string.value };
	Variable_t* iter = NULL;
	iter = copyVariableToPtr(newIntVariable(0));
	iter->gcDoNotFree = 1;
	runtimeVariableEdit(&setVar, iter);

	for (int i = 0; i < v->count; i++) {
		*iter = arrayClassGetIdx(caller, i);
		iter->gcDoNotFree = 1;

		Variable_t* res = genericCallDirect(args[1], NULL, 0);
		if (res == NULL) {
			if (scriptLastError == SCRIPT_BREAK) {
				break;
			}
			else {
				return NULL;
			}
		}
	}

	iter->reference = 1;
	free(iter);

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
	if (v->count < idx || idx < 0) {
		SCRIPT_FATAL_ERR("Accessing index %d while array is %d long", (int)idx, (int)caller->solvedArray.vector.count);
	}

	u32 oldCount = caller->solvedArray.vector.count;
	caller->solvedArray.vector.count = idx;
	if (arrayClassAdd(caller, args[1])){
		SCRIPT_FATAL_ERR("Adding the wrong type to a typed array");
	}
	caller->solvedArray.vector.count = oldCount;

	return &emptyClass;
}

ClassFunction(arrayAdd) {
	Variable_t* arg = *args;

	if (caller->readOnly) {
		SCRIPT_FATAL_ERR("Array is read-only");
	}

	if (arrayClassAdd(caller, arg)){
		SCRIPT_FATAL_ERR("Adding the wrong type to a typed array");
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
		SCRIPT_FATAL_ERR("Need a bytearray to convert to str");
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
	{"slice", arraySlice, 2, doubleInt},
	{"foreach", arrayForEach, 2, oneStringoneFunction},
	{"copy", arrayCopy, 0, 0},
	{"set", arraySet, 2, oneIntOneAny},
	{"+", arrayAdd, 1, anotherAnotherOneVarArg},
	{"-", arrayMinus, 1, anotherOneIntArg},
	{"contains", arrayContains, 1, anotherAnotherOneVarArg},
	{"bytestostr", bytesToStr, 0, 0},
	{"==", eqArray, 1, oneByteArrayClass},
	{"==", eqArray, 1, oneIntArrayClass},
};

Variable_t getArrayMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, arrayFunctions, ARRAY_SIZE(arrayFunctions));
}