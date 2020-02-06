#pragma once
#define strcmpcheck(x, y) (!strcmp(x, y))

typedef int (*part_handler)();
typedef struct _script_parts {
    char name[11];
    part_handler handler;
    short arg_amount;
} script_parts;

void ParseScript(char* path);
int Part_WaitOnUser();
int Part_Exit();
int Part_ErrorPrint();
int Part_Print();
int Part_MountMMC();
int Part_ConnectMMC();
int Part_MakeFolder();
int Part_RecursiveCopy();
int Testing_Part_Handler();
int Part_Copy();