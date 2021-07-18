
#pragma once
#include "compat.h"

typedef struct {
	Function_t main;
	Vector_t staticVarHolder;
	u8 valid;
} ParserRet_t;

#define SCRIPT_PARSER_ERR(message, ...) printScriptError(SCRIPT_PARSER_FATAL, message, ##__VA_ARGS__); return (ParserRet_t){0}

void exitStaticVars(Vector_t* v);
void exitFunction(Operator_t* start, u32 len);
ParserRet_t parseScript(char* in, u32 len);