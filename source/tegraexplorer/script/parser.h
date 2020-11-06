#pragma once

#include "types.h"
//#define DictValue(type, val, len) (dictValue_t) {type, len, val}

#define scriptResultCreate(resCode, funcName) (scriptResult_t) {resCode, funcName}

void startScript(char* path);
char* readFile(char* path);