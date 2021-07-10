#include "scriptError.h"
#include "compat.h"
#include <stdarg.h>

s64 scriptCurrentLine;
u8 scriptLastError = 0;

void printScriptError(u8 errLevel, char* message, ...) {
	va_list args;
	scriptLastError = errLevel;
	va_start(args, message);
	gfx_printf("\n\n[%s] ", (errLevel == SCRIPT_FATAL) ? "FATAL" : (errLevel == SCRIPT_PARSER_FATAL) ? "PARSE_FATAL" : "WARN");
	gfx_vprintf(message, args);
	if (errLevel < SCRIPT_WARN)
		gfx_printf("\nError occured on or near line %d\n", scriptCurrentLine);
	va_end(args);
}