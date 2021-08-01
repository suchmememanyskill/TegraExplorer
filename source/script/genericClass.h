#pragma once
#include "model.h"

Variable_t* copyVariableToPtr(Variable_t var);

#define VARARGCOUNT 255

#define ClassFunction(name) Variable_t* name(Variable_t* caller, Variable_t** args, u8 argsLen)


typedef Variable_t (*getMemberFunction)(Variable_t*, char*);

typedef struct {
	u8 classType;
	getMemberFunction func;
} MemberGetters_t;

Variable_t getGenericFunctionMember(Variable_t* var, char* memberName, ClassFunctionTableEntry_t* entries, u8 len);
Variable_t* genericGet(Variable_t* var, CallArgs_t* ref);
Variable_t* genericCallDirect(Variable_t* var, Variable_t** args, u8 len);
Variable_t* callMemberFunctionDirect(Variable_t* var, char* memberName, Variable_t** args, u8 argsLen);
Variable_t* genericCall(Variable_t* var, CallArgs_t* ref);
void freeVariable(Variable_t** target);
Variable_t* callMemberFunction(Variable_t* var, char* memberName, CallArgs_t* args);
void freeVariableInternal(Variable_t* referencedTarget);