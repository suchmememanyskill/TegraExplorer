#pragma once
#include "model.h"

enum {
	SCRIPT_FATAL = 0,
	SCRIPT_PARSER_FATAL,
	SCRIPT_WARN,
	SCRIPT_BREAK,
};

extern s64 scriptCurrentLine;
extern u8 scriptLastError;

void printScriptError(u8 errLevel, char* message, ...);

#define SCRIPT_FATAL_ERR(message, ...) printScriptError(SCRIPT_FATAL, message, ##__VA_ARGS__); return NULL
#define SCRIPT_WARN_ERR(message, ...) printScriptError(SCRIPT_WARN, message, ##__VA_ARGS__)

