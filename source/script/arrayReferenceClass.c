#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "intClass.h"
#include "arrayReferenceClass.h"


ClassFunction(projectArray) {
	Variable_t newArray = { .variableType = caller->solvedArray.arrayClassReference->variableType, .reference = 1, .readOnly = 1 };

	newArray.solvedArray.vector = caller->solvedArray.arrayClassReference->solvedArray.vector;
	newArray.solvedArray.vector.data = (u8*)caller->solvedArray.arrayClassReference->solvedArray.vector.data + (caller->solvedArray.offset * caller->solvedArray.arrayClassReference->solvedArray.vector.elemSz);
	newArray.solvedArray.vector.count = caller->solvedArray.len;

	return copyVariableToPtr(newArray);
}

ClassFunctionTableEntry_t arrayReferenceFunctions[] = {
	{"project", projectArray, 0, 0},
};

Variable_t getArrayReferenceMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, arrayReferenceFunctions, ARRAY_SIZE(arrayReferenceFunctions));
}