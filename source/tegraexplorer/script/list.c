#include "list.h"
#include <string.h> 
#include "types.h"
#include "../../mem/heap.h"

void freeDictValue(dictValue_t dv) {
	if (dv.string == NULL)
		return;

	switch (dv.varType) {
		case StringType:
		case IntArrayType:
		case ByteArrayType: // As it's a union with the biggest object being a pointer, this should be the same
			free(dv.string);
			break;
		case StringArrayType:
			for (unsigned int i = 0; i < dv.arrayLen; i++)
				free(dv.stringArray[i]);

			free(dv.stringArray);
	}
}

void freeDict(dict_t* dict) {
	freeDictValue(dict->value);
	free(dict->key);
	free(dict);
}

varVector_t varVectorInit(int startSize) {
	varVector_t vec = { 0 };

	vec.vars = calloc(startSize, sizeof(dict_t));
	vec.len = startSize;
	vec.stored = 0;
	return vec;
}

dictValue_t* varVectorFind(varVector_t* vec, const char* key) {
	for (int i = 0; i < vec->stored; i++) {
		if (!strcmp(vec->vars[i].key, key))
			return &vec->vars[i].value;
	}

	return NULL;
}

void varVectorAdd(varVector_t* vec, dict_t add) {
	dictValue_t* var = NULL;
	if (add.key != NULL)
		var = varVectorFind(vec, add.key);

	if (var != NULL) {
		freeDictValue(*var);
		*var = add.value;
		free(add.key);
		return;
	}

	if (vec->stored >= vec->len) {
		vec->len *= 2;
		dict_t* old = vec->vars;
		//vec->vars = realloc(old, vec->len * sizeof(dict_t));
		void *temp = calloc(vec->len, sizeof(dict_t));
		memcpy(temp, vec->vars, vec->len / 2 * sizeof(dict_t));
		free(vec->vars);
		vec->vars = temp;
		if (vec->vars == NULL) {
			free(old);
		}		
	}

	vec->vars[vec->stored] = add;
	vec->stored++;
}

void varVectorFree(varVector_t* vec) {
	for (int i = 0; i < vec->stored; i++) {
		dict_t *var = &vec->vars[i];
		if (var->key != NULL) {
			free(var->key);
			var->key = NULL;
		}
		if (var->value.free)
			freeDictValue(var->value);
	}
	free(vec->vars);
}