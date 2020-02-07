#pragma once
#define strcmpcheck(x, y) (!strcmp(x, y))

typedef int (*part_handler)();
typedef struct _script_parts {
    char name[11];
    part_handler handler;
    short arg_amount;
} script_parts;

void ParseScript(char* path);
