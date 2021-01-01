#pragma once
#include <utils/types.h>

char *CpyStr(const char* in);
void MaskIn(char *mod, u32 bitstream, char mask);
bool StrEndsWith(char *begin, char *end);
void WaitFor(u32 ms);
void RebootToPayloadOrRcm();

#define FREE(x) free(x); x = NULL;
char *ShowKeyboard(const char *toEdit, u8 alwaysRet);