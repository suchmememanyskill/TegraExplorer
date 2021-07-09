#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef long long s64;



#ifdef WIN32
#pragma pack(1)
typedef struct {
	void* data;
	u32 capacity;
	u32 count;
	u8 elemSz;
} Vector_t;
#else
#include "../utils/vector.h"
#include <libs/nx_savedata/save.h>
#pragma pack(1)
#endif


typedef enum {
	None = 0,
	IntClass,
	FunctionClass,
	StringClass,
	ByteArrayClass,
	StringArrayClass,
	IntArrayClass,
	UnresolvedArrayClass,
	DictionaryClass,
	EmptyClass,
	SolvedArrayReferenceClass,
	SaveClass,
} VariableType_t;

typedef enum {
	Invalid = 0,
	Variable,
	BetweenBrackets,
	Not,

	Plus,
	Equals,
	Minus,
	Multiply,
	Division,
	Modulo,

	LeftSquareBracket,
	LeftCurlyBracket,
	LeftBracket,
	RightSquareBracket,
	RightCurlyBracket,
	RightBracket,

	Smaller,
	SmallerEqual,
	Bigger,
	BiggerEqual,
	EqualEqual,
	NotEqual,
	LogicAnd,
	LogicOr,

	BitShiftLeft,
	BitShiftRight,
	And,
	Or,

	EquationSeperator,
	Dot,
	CallArgs,
} Token_t;

typedef enum {
	ActionGet = 0,
	ActionSet,
	ActionCall,
} ActionType_t;

typedef enum {
	ActionExtraNone = 0,
	ActionExtraArrayIndex,
	ActionExtraMemberName,
	ActionExtraCallArgs,
	ActionExtraCallArgsFunction
} ActionExtraType_t;


// Change to a Vector_t with Operator_t's
typedef struct {
	Vector_t operations; // Operation_t. Equations seperated by EquationSep
} Function_t;

struct _ClassFunctionTableEntry_t;
struct _Variable_t;

typedef struct _FunctionClass_t {
	union {
		struct {
			u8 builtIn : 1;
			u8 firstArgAsFunction : 1;
		};
		u8 unionFunctionOptions;
	};
	union {
		Function_t function;
		struct {
			struct _ClassFunctionTableEntry_t* builtInPtr;
			struct _Variable_t* origin;
			u8 len;
		};
		
	};

} FunctionClass_t;

typedef enum {
	ArrayType_Int = 0,
	ArrayType_String,
	ArrayType_Byte
} ArrayType_t;

typedef struct {
	union {
		Vector_t vector; // vector of typeof(value)
		struct {
			struct _Variable_t* arrayClassReference;
			u32 offset;
			u32 len;
		};
	};
	
} ArrayClass_t;

typedef struct _UnsolvedArrayClass_t {
	Vector_t operations; // Operator_t
} UnsolvedArrayClass_t;

typedef struct _DictionaryClass_t {
	Vector_t vector; // vector of typeof(Dict_t)
} DictionaryClass_t;

typedef struct _IntClass_t {
	s64 value;
} IntClass_t;

typedef struct _StringClass_t {
	char* value;
	struct {
		u8 free : 1;
	};
} StringClass_t;

#ifndef WIN32 
typedef struct {
	save_ctx_t saveCtx;
	FIL saveFile;
} SaveClass_t;
#endif 

typedef struct _Variable_t {
	//void* variable;
	union {
		FunctionClass_t function;
		UnsolvedArrayClass_t unsolvedArray;
		DictionaryClass_t dictionary;
		IntClass_t integer;
		StringClass_t string;
		ArrayClass_t solvedArray;
		#ifndef WIN32
		SaveClass_t *save;
		#endif
	};
	union {
		struct {
			u8 variableType : 5;
			u8 readOnly : 1;
			u8 reference : 1;
			u8 gcDoNotFree : 1;
		};
	};
} Variable_t;

typedef struct _CallArgs_t {
	struct {
		u8 action : 4;
		u8 extraAction : 4;
	};
	void* extra; // Function_t for arrayIdx, char* for member, Function_t for funcCall, Function_t x2 for funcCallArgs
	struct _CallArgs_t* next;
} CallArgs_t;

typedef struct {
	void* data;
	u32 len;
} Array_t;

typedef Variable_t* (*ClassFunction)(Variable_t* caller, Variable_t** args, u8 argsLen);

typedef struct _ClassFunctionTableEntry_t {
	char* name;
	ClassFunction func;
	u8 argCount;
	u8* argTypes;
} ClassFunctionTableEntry_t;

typedef struct _VariableReference_t {
	union {
		struct {
			u8 staticVariableSet : 1;
			u8 staticVariableRef : 1;
			u8 staticVariableType : 2; // 0 = ref, 1 = int, 2 = str, 3 = stdlib func
		};
		u8 staticVariableOptionsUnion;
	};

	union {
		Variable_t* staticVariable;
		char* name;
		Array_t betweenBrackets;
		s64 integerType;
		char* stringType;
		ClassFunctionTableEntry_t* staticFunction;
	};
} VariableReference_t;

//typedef Variable_t* (*classFunctionTable)(VariableReference_t*);

typedef Variable_t* (*classFunctionTable)(char*, Variable_t*, VariableReference_t*, Vector_t*);

typedef struct {
	char* name;
	Variable_t* var;
} Dict_t;

typedef struct {
	u8 token : 8;
	char strToken[3];
} TokenConvertion_t;

extern TokenConvertion_t tokenConvertions[];
extern u32 tokenConvertionCount;

typedef struct {
	Vector_t operations; // Operator_t
} Equation_t;

typedef struct {
	struct {
		u8 token : 7;
		u8 not : 1;
	};
	union {
		VariableReference_t variable;
		CallArgs_t callArgs;
		char* tokenStr;
		s64 lineNumber;
	};
	// probably should add u16 lineNum here
} Operator_t;

extern Variable_t emptyClass;

#pragma pack()