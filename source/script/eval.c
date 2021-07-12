#include "model.h"
#include "compat.h"
#include "genericClass.h"
#include "eval.h"
#include "garbageCollector.h"
#include "standardLibrary.h"
#include "scriptError.h"
#include "StringClass.h"
#include "intClass.h"
#include "unsolvedArrayClass.h"
#include "functionClass.h"
#include <string.h>

Variable_t* staticVars;

void setStaticVars(Vector_t* vec) {
	staticVars = vec->data;
}

Vector_t runtimeVars;

void initRuntimeVars() {
	runtimeVars = newVec(sizeof(Dict_t), 8);
}

void exitRuntimeVars() {
	vecForEach(Dict_t*, runtimeVar, (&runtimeVars)) {
		FREE(runtimeVar->name);
		removePendingReference(runtimeVar->var);
	}

	vecFree(runtimeVars);
}

Variable_t* opToVar(Operator_t* op, Callback_SetVar_t *setCallback) {
	Variable_t* var = NULL;
	CallArgs_t* args = NULL;

	if ((op + 1)->token == CallArgs)
		args = &(op + 1)->callArgs;

	if (op->token == BetweenBrackets) {
		var = eval(op->variable.betweenBrackets.data, op->variable.betweenBrackets.len, 1);
	}
	else if (op->token == Variable) {
		if (op->variable.staticVariableSet) {
			if (op->variable.staticVariableRef) {
				op->variable.staticVariable = &staticVars[(int)op->variable.staticVariable];
				op->variable.staticVariableRef = 0;
				op->variable.staticVariable->readOnly = 1;
				op->variable.staticVariable->reference = 1;
				op->variable.staticVariable->gcDoNotFree = 1;
			}

			var = op->variable.staticVariable;

			if (var->variableType == UnresolvedArrayClass) {
				var = solveArray(var);
			}
		}
		else if (op->variable.staticVariableType > 0) {
			if (op->variable.staticVariableType == 1) {
				var = copyVariableToPtr(newIntVariable(op->variable.integerType));
			}
			else if (op->variable.staticVariableType == 2) {
				var = copyVariableToPtr(newStringVariable(op->variable.stringType, 1, 0));
				var->reference = 1;
			}
			else if (op->variable.staticVariableType == 3) {
				var = copyVariableToPtr(newFunctionVariable(createFunctionClass((Function_t) { 0 }, op->variable.staticFunction)));
				var->reference = 1;

				if (!strcmp(var->function.builtInPtr->name,"while")) {
					var->function.firstArgAsFunction = 1;
				}
			}
		}
		else {
			if (args != NULL) {
				if (args->action == ActionSet && args->extraAction == ActionExtraNone) {
					setCallback->isTopLevel = 1;
					setCallback->varName = op->variable.name;
					setCallback->hasVarName = 1;
					return NULL;
				}
			}

			vecForEach(Dict_t*, variableArrayEntry, (&runtimeVars)) {
				if (!strcmp(variableArrayEntry->name, op->variable.name)) {
					var = variableArrayEntry->var;
					break;
				}
			}

			if (var == NULL) {
				SCRIPT_FATAL_ERR("Variable '%s' not found", op->variable.name);
			}

			addPendingReference(var);
		}
	}

	while (args) {
		Variable_t* varNext = NULL;
		if (args->action == ActionGet) {
			if (args->extraAction == ActionExtraMemberName && args->next != NULL && args->next->action == ActionCall) {
				varNext = callMemberFunction(var, args->extra, args->next);
				args = args->next;
			}
			else {
				varNext = genericGet(var, args);
			}
		}
		else if (args->action == ActionSet) {
			if (var->readOnly) {
				SCRIPT_FATAL_ERR("Variable which set was called on is read-only");
				return NULL;
			}

			if (args->extraAction == ActionExtraMemberName || args->extraAction == ActionExtraArrayIndex) {
				setCallback->hasVarName = (args->extraAction == ActionExtraMemberName) ? 1 : 0;
				setCallback->setVar = var;
				if (args->extraAction == ActionExtraMemberName) {
					setCallback->varName = args->extra;
				}
				else {
					setCallback->idxVar = args->extra;
				}
			}
			else {
				SCRIPT_FATAL_ERR("[FATAL] Unexpected set!");
			}
			return NULL;
		}
		else if (args->action == ActionCall) {
			varNext = genericCall(var, args);
		}

		if (varNext == NULL)
			return NULL;

		removePendingReference(var);

		//if (!var->reference)
		//	freeVariable(&var);

		var = varNext;
		args = args->next;
	}

	if (op->not && var) {
		Variable_t* newVar = callMemberFunctionDirect(var, "not", NULL, 0);
		removePendingReference(var);
		var = newVar;
	}
	
	return var;
}

void runtimeVariableEdit(Callback_SetVar_t* set, Variable_t* curRes) {
	if (set->isTopLevel) {
		vecForEach(Dict_t*, variableArrayEntry, (&runtimeVars)) {
			if (!strcmp(variableArrayEntry->name, set->varName)) {
				removePendingReference(variableArrayEntry->var);
				//addPendingReference(curRes);
				variableArrayEntry->var = curRes;
				return;
			}
		}

		Dict_t newStoredVariable = { 0 };
		newStoredVariable.name = CpyStr(set->varName);
		newStoredVariable.var = curRes;
		vecAdd(&runtimeVars, newStoredVariable);
		return;
	}

	if (set->idxVar) {
		Variable_t* var = eval(set->idxVar->operations.data, set->idxVar->operations.count, 1);
		Variable_t* args[2] = { var, curRes };
		callMemberFunctionDirect(set->setVar, "set", args, 2);
		removePendingReference(var);
	}
	else {
		Variable_t varName = { .variableType = StringClass, .reference = 1, .string.value = set->varName };
		Variable_t* args[2] = { &varName, curRes };
		callMemberFunctionDirect(set->setVar, "set", args, 2);
	}

	removePendingReference(curRes);
	removePendingReference(set->setVar);

	// TODO: add non-top level sets
}

Variable_t* eval(Operator_t* ops, u32 len, u8 ret) {
	Variable_t* curRes = NULL;
	Operator_t* curOp = NULL;
	Callback_SetVar_t set = { 0 };
	for (u32 i = 0; i < len; i++) {
		Operator_t* cur = &ops[i];

		if (cur->token == CallArgs)
			continue;

		if (cur->token == EquationSeperator) {
			scriptCurrentLine = cur->lineNumber;
			if (set.hasBeenNoticed == 1) 
				runtimeVariableEdit(&set, curRes);
			else
				removePendingReference(curRes);

			memset(&set, 0, sizeof(Callback_SetVar_t));
			curRes = NULL;
			curOp = NULL;
			continue;
		}

		if (curRes == NULL) {
			if (cur->token != Variable && cur->token != BetweenBrackets) {
				SCRIPT_FATAL_ERR("First token is not a variable");
			}
			else {
				curRes = opToVar(cur, &set);
				if (!curRes) {
					if ((set.varName != NULL || set.idxVar != NULL) && set.hasBeenNoticed == 0) {
						set.hasBeenNoticed = 1;
						continue;
					}
					return NULL;
				}
			}
			continue;
		}

		if (curOp == NULL) {
			if (cur->token != Variable && cur->token != BetweenBrackets) {
				curOp = cur;
			}
			else {
				SCRIPT_FATAL_ERR("Expected operator");
			}
			continue;
		}

		Variable_t* rightSide = opToVar(cur, &set);
		if (!rightSide) 
			return NULL;
		

		// Issue lies here for freeing issues, curRes is corrupted
		Variable_t* result = callMemberFunctionDirect(curRes, curOp->tokenStr, &rightSide, 1);
		// Free old values

		removePendingReference(curRes);
		removePendingReference(rightSide);
		rightSide = NULL;
		curOp = NULL;

		curRes = result;
	}

	if (set.hasBeenNoticed == 1) {
		runtimeVariableEdit(&set, curRes);
		return &emptyClass;
	}
	else if (!ret) {
		removePendingReference(curRes);
		return &emptyClass;
	}

	return curRes;
}