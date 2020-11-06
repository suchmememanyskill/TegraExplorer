#pragma once
#include "types.h"

#define dictValSetInt(dictVal, i) dictVal.integer = i; dictVal.type = IntType;
#define dictValSetStr(dictVal, str) dictVal.string = strdup(str); dictVal.type = StringType;
#define dictValSetJump(dictVal, i) dictVal.integer = i; dictVal.type = JumpType;

#define dictValInt(i) (dictValue_t) {IntType, 0, i}

#define DictCreate(key, value) (dict_t) {key, value}
#define DictValueCreate(type, arrayLen, val) (dictValue_t) {type, arrayLen, val}

void freeDictValue(dictValue_t dv);
void freeDict(dict_t* dict);

varVector_t varVectorInit(int startSize);
void varVectorAdd(varVector_t* vec, dict_t add);
dictValue_t* varVectorFind(varVector_t* vec, const char* key);

static inline void freeDictValueOnVar(dictValue_t dv) {
	if (dv.free)
		freeDictValue(dv);
}

void varVectorFree(varVector_t* vec);