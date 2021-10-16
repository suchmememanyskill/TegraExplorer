#include "unsolvedArrayClass.h"
#include "eval.h"
#include "compat.h"
#include "intClass.h"
#include "scriptError.h"
#include "garbageCollector.h"
#include <string.h>

Variable_t* solveArray(Variable_t *unsolvedArray) {
	if (unsolvedArray->unsolvedArray.operations.count <= 0) {
		return copyVariableToPtr(*unsolvedArray);
		// Return empty unsolved array that turns into a solved array once something is put into it
	}

		int lasti = 0;
		Operator_t* ops = unsolvedArray->unsolvedArray.operations.data;
		u8 type = None;
		Vector_t v = { 0 };


		for (int i = 0; i < unsolvedArray->unsolvedArray.operations.count; i++) {
			if (ops[i].token == EquationSeperator || i + 1 == unsolvedArray->unsolvedArray.operations.count) {
				if (i + 1 == unsolvedArray->unsolvedArray.operations.count)
					i++;

				Variable_t* var = eval(&ops[lasti], i - lasti, 1);
				if (var == NULL)
					return NULL; 

				if (v.data == NULL) {
					if (var->variableType == IntClass) {
						v = newVec(sizeof(s64), 1);
						type = IntClass;
					}
					else if (var->variableType == StringClass) {
						v = newVec(sizeof(char*), 1);
						type = StringClass;
					}
					else {
						SCRIPT_FATAL_ERR("Unknown array type");
					}
				}

				if (var->variableType == type) {
					if (var->variableType == IntClass) {
						vecAdd(&v, var->integer.value);
					}
					else {
						char* str = CpyStr(var->string.value);
						vecAdd(&v, str);
					}
				}
				else {
					SCRIPT_FATAL_ERR("Variable type is not the same as array type");
				}

				removePendingReference(var);

				lasti = i + 1;
			}
		}

		Variable_t arrayVar = { .variableType = (type == IntClass) ? IntArrayClass : StringArrayClass, .solvedArray.vector = v };
		return copyVariableToPtr(arrayVar);
}

Variable_t createUnsolvedArrayVariable(Function_t* f) {
	Variable_t var = { 0 };
	Vector_t holder = { 0 };
	u8 varType = Invalid;
	
	// Foreach to attempt to create the array. Should fail if calcs are done or types are not equal
	if (f->operations.count > 0) {
		vecForEach(Operator_t*, curOp, (&f->operations)) {
			if (holder.data == NULL) {
				if (curOp->variable.staticVariableType == 1) {
					varType = IntClass;
					holder = newVec(sizeof(s64), 4);
					vecAdd(&holder, (curOp->variable.integerType));
				}
				else if (curOp->variable.staticVariableType == 2) {
					if (!strcmp(curOp->variable.stringType, "BYTE[]")) {
						varType = ByteArrayClass; // Repurpose varType
						holder = newVec(sizeof(u8), 4);
						FREE(curOp->variable.stringType);
					}
					else {
						varType = StringClass;
						holder = newVec(sizeof(char*), 4);
						vecAdd(&holder, (curOp->variable.stringType));
					}
				}
				else {
					break;
				}
			}
			else {
				if ((curOp - 1)->token == EquationSeperator && curOp->token == Variable) {
					if (curOp->variable.staticVariableType == 1) {
						if (varType == IntClass) {
							vecAdd(&holder, curOp->variable.integerType);
						}
						else if (varType == ByteArrayClass) {
							u8 var = (u8)(curOp->variable.integerType & 0xFF);
							vecAdd(&holder, var);
						}
					}
					else if (curOp->variable.staticVariableType == 2) {
						if (varType == StringClass) {
							vecAdd(&holder, curOp->variable.stringType);
						}
					}
					else {
						vecFree(holder);
						holder.data = NULL;
						break;
					}
				}
				else if (curOp->token == EquationSeperator) {
					continue;
				}
				else {
					vecFree(holder);
					holder.data = NULL;
					break;
				}
			}
		}
	}

	if (varType != Invalid) {
		if (varType == IntClass) {
			var.variableType = IntArrayClass;
		}
		else if (varType == StringClass) {
			var.variableType = StringArrayClass;
		}
		else {
			var.variableType = varType;
		}

		vecFree(f->operations);
		var.solvedArray.vector = holder;
		var.readOnly = 1;
	}
	else {
		var.unsolvedArray.operations = f->operations;
		var.variableType = UnresolvedArrayClass;
	}


	return var;
}

ClassFunction(retZero){
	return newIntVariablePtr(0);
}

u8 anotherOneVarArg[] = { VARARGCOUNT };
u8 unsolvedArrayStr[] = { StringClass };

ClassFunction(createTypedArray) {
	Vector_t v = { 0 };
	Variable_t* arg = *args;

	if (arg->variableType == IntClass) {
		v = newVec(sizeof(s64), 1);
		vecAdd(&v, arg->integer.value);
	}
	else if (arg->variableType == StringClass) {
		v = newVec(sizeof(char*), 1);
		char* str = CpyStr(arg->string.value);
		vecAdd(&v, str);
	}
	else {
		SCRIPT_FATAL_ERR("Unknown array type");
	}

	Variable_t arrayVar = { .variableType = (arg->variableType == IntClass) ? IntArrayClass : StringArrayClass, .solvedArray.vector = v };
	*caller = arrayVar;
	
	return &emptyClass;
}

ClassFunctionTableEntry_t unsolvedArrayFunctions[] = {
	{"+", createTypedArray, 1, anotherOneVarArg},
	{"add", createTypedArray, 1, anotherOneVarArg},
	{"len", retZero, 0, 0},
};

Variable_t getUnsolvedArrayMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, unsolvedArrayFunctions, ARRAY_SIZE(unsolvedArrayFunctions));
}