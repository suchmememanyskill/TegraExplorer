#include "variables.h"
#include "types.h"
#include <string.h>
#include <mem/heap.h>

void freeVariable(Variable_t dv) {
	if (!dv.free)
		return;

	switch (dv.varType) {
		case StringType:
			FREE(dv.stringType);
			break;

		case StringArrayType:;
			char** strArray = vecGetArray(char**, dv.vectorType);
			for (u32 i = 0; i < dv.vectorType.count; i++){
				free(strArray[i]);
			}	

		case IntArrayType:
		case ByteArrayType:
			vecFree(dv.vectorType);
			break;
	}
}

void freeVariableVector(Vector_t *v) {
	Variable_t* vars = vecGetArrayPtr(v, Variable_t*);
	for (int i = 0; i < v->count; i++) {
		freeVariable(vars[i]);
	}
	vecFreePtr(v);
}

void freeDictVector(Vector_t *v) {
	dict_t* dic = vecGetArrayPtr(v, dict_t*);
	for (int i = 0; i < v->count; i++) {
		FREE(dic[i].key);
		freeVariable(dic[i].value);
	}
	vecFreePtr(v);
}

Variable_t* dictVectorFind(Vector_t* v, const char* key) {
	dict_t* dic = vecGetArrayPtr(v, dict_t*);
	for (int i = 0; i < v->count; i++) {
		if (!strcmp(dic[i].key, key))
			return &dic[i].value;
	}

	return NULL;
}

void dictVectorAdd(Vector_t* v, dict_t add) {
	if (add.key == NULL)
		return;

	Variable_t* var = dictVectorFind(v, add.key);

	if (var != NULL) {
		if ((var->varType >= StringType && var->varType <= ByteArrayType && var->varType == add.value.varType) && (add.value.stringType == var->stringType || add.value.vectorType.data == var->vectorType.data))
			return;

		freeVariable(*var);
		*var = add.value;
		free(add.key);
		return;
	}
	else {
		vecAddElement(v, add);
	}
}

scriptCtx_t createScriptCtx() {
	scriptCtx_t s = {
		.indentInstructors = newVec(sizeof(indentInstructor_t), 64),
		.varDict = newVec(sizeof(dict_t), 8),
		.startEquation = 0,
		.curPos = 0
	};
	return s;
}

// We should rewrite this to actually use the vector!
u8 setIndentInstruction(scriptCtx_t* ctx, u8 level, u8 skip, u8 func, int jumpLoc) {
	if (level >= 64)
		return 1;

	indentInstructor_t* instructors = vecGetArray(indentInstructor_t*, ctx->indentInstructors);
	indentInstructor_t* instructor = &instructors[level];

	instructor->skip = skip;
	instructor->active = 1;
	instructor->function = func;
	instructor->jump = (jumpLoc >= 0) ? 1 : 0;
	instructor->jumpLoc = jumpLoc;

	return 0;
}

indentInstructor_t* getCurIndentInstruction(scriptCtx_t* ctx) {
	indentInstructor_t* instructors = vecGetArray(indentInstructor_t*, ctx->indentInstructors);
	return &instructors[ctx->indentIndex];
}