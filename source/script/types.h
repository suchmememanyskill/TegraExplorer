#pragma once

#include <utils/types.h>
#include "../utils/vector.h"

enum Tokens {
	Invalid = 0,
	Variable = 1,
	ArrayVariable,	
	Function,
	LBracket,
	StrLit,
	IntLit,
	LSBracket,
	VariableAssignment,
	ArrayVariableAssignment,
	FunctionAssignment,
	RBracket,
	RCBracket,
	LCBracket,
	Seperator,
	Plus,
	Minus,
	Multiply,
	Division,
	Mod,
	Smaller,
	SmallerEqual,
	Bigger,
	BiggerEqual,
	Equal,
	EqualEqual,
	Not,
	NotEqual,
	LogicAND,
	LogicOR,
	RSBracket,
	AND,
	OR,
	EquationSeperator,
};

typedef struct {
	u8 token;
	union {
		char* text;
		int val;
	};
} lexarToken_t;

enum Errors {
	ERRBADOPERATOR = 1,
	ERRDOUBLENOT,
	ERRSYNTAX,
	ERRINVALIDTYPE,
	ERRNOVAR,
	ERRNOFUNC,
	ERRINACTIVEINDENT,
	ERRDIVBYZERO
};

enum Variables {
	IntType = 0,
	StringType,
	IntArrayType,
	StringArrayType,
	ByteArrayType,
	JumpType,
	DictType,
	NullType,
	ErrType,
};

typedef struct { // this is to keep track of how many {} we passed. Keep an internal var with the "indentation level", +1 for {, -1 for }. have an array with the following def on what to do (on func: enter, set indentation & jump back, on while, jump to while, use while as if, on if simply set true or false)
	union {
		struct {
			u8 active : 1;
			u8 skip : 1;
			u8 jump : 1;
			u8 function : 1;
		};
		u8 container;
	};
	int jumpLoc;
} indentInstructor_t;

typedef struct {
	union {
		struct {
			u8 varType : 7;
			u8 free : 1;
		};
		u8 typeUnion;
	};
	union {
		int integerType;
		char* stringType;
		Vector_t vectorType;
	};
} Variable_t;

typedef struct {
	char* key;
	Variable_t value;
} dict_t;

typedef struct {
	Vector_t indentInstructors; // Type indentInstructor_t
	Vector_t varDict; // Type dict_t
	Vector_t script; // Type lexarToken_t
	u32 startEquation;
	u32 curPos;
	u8 indentIndex;
} scriptCtx_t;

typedef Variable_t(*func_int_ptr)(scriptCtx_t* ctx, Variable_t *vars, u32 len);

typedef struct {
	char* key;
	func_int_ptr value;
	u8 argCount;
	u8* typeArray;
} functionStruct_t;

typedef struct {
	int resCode;
	lexarToken_t* nearToken;
	u32 len;
} scriptResult_t;



#define newDict(strName, var) (dict_t) {strName, var}
#define newVar(var, frii, value) (Variable_t) {.varType = var, .free = frii, value}

#define varArgs 255

#define NullVar newVar(NullType, 0, 0)
#define ErrVar(err) newVar(ErrType, 0, err)
#define IntVal(val) newVar(IntType, 0, val)