#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct {
	u16 token;
	union {
		char* text;
		int val;
	};
} lexarToken_t;

typedef struct {
	lexarToken_t* tokens;
	u32 len;
	u32 stored;
} lexarVector_t;

enum Tokens {
	Invalid = 0,
	Variable = 1,
	LBracket,
	RBracket,
	RCBracket,
	LCBracket,
	StrLit,
	IntLit,
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
	LSBracket,
	RSBracket,
	AND,
	OR
};

enum Errors {
	ERRBADOPERATOR = 1,
	ERRDOUBLENOT,
	ERRSYNTAX,
	ERRINVALIDTYPE,
	ERRNOVAR,
	ERRNOFUNC,
	ERRINACTIVEINDENT
};

typedef struct { // this is to keep track of how many {} we passed. Keep an internal var with the "indentation level", +1 for {, -1 for }. have an array with the following def on what to do (on func: enter, set indentation & jump back, on while, jump to while, use while as if, on if simply set true or false)
	union {
		struct {
			u8 active : 1;
			u8 skip : 1;
			u8 jump : 1;
		};
		u8 container;
	};
	int jumpLoc;
} indentInstructor_t;

typedef struct {
	char* start;
	u32 len;
	union {
		struct {
			u8 set : 1;
		};
		u8 options;
	};
} textHolder_t;

typedef struct {
	int resCode;
	lexarToken_t* nearToken;
} scriptResult_t;

#define JumpDictValue(jump) (dictValue_t) {{JumpType}, 0, .integer = jump}
#define StrDictValue(str) (dictValue_t) {{StringType}, 0, .string = str}
#define ErrDictValue(err) (dictValue_t) {{ErrType}, 0, .integer = err}
#define NullDictValue() (dictValue_t) {{NullType}, 0, .integer = 0}
#define IntArrDictValue(arr, len) (dictValue_t) {{IntArrayType}, len, .integerArray = arr}
#define StringArrDictValue(arr, len) (dictValue_t) {{StringArrayType}, len, .stringArray = arr}
#define IntDictValue(i) (dictValue_t) {{IntType}, 0, .integer = i}

enum {
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

typedef struct {
	union {
		u8 type;
		struct {
			u8 varType : 7;
			u8 free : 1;
		};
	};
	unsigned int arrayLen;
	union {
		int integer;
		char* string;
		unsigned char* byteArray;
		int* integerArray;
		char** stringArray;
	};
} dictValue_t;

typedef struct {
	char* key;
	dictValue_t value;
} dict_t;

typedef struct {
	dict_t* vars;
	u32 len;
	u32 stored;
} varVector_t;

typedef struct {
	indentInstructor_t* indentInstructors;
	u8 indentLevel; // -> max 63
	varVector_t vars;
	lexarVector_t* script;
	lexarToken_t lastToken;
	lexarToken_t varToken;
	u32 startEq;
	u32 curPos;
	u32 lastTokenPos;
	lexarVector_t args_loc;
	union {
		u32 varIndexStruct;
		struct {
			u32 varIndex : 31;
			u32 varIndexSet : 1;
		};
	};
} scriptCtx_t;

typedef dictValue_t(*func_int_ptr)(scriptCtx_t* ctx, varVector_t* args);

typedef struct {
	char* key;
	func_int_ptr value;
	union {
		u8 args;
		struct {
			u8 argCount : 6;
			u8 varArgs : 1;
			u8 operatorParse : 1;
		};
	};
} str_fnc_struct;

#define FREEINDICT 0x80