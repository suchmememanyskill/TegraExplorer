#pragma once
#include "../../utils/types.h"

typedef struct _dict_str_int {
    char *key;
    int value;
    struct _dict_str_int *next;
} dict_str_int;

typedef struct _dict_str_str {
    char *key;
    char *value;
    struct _dict_str_str *next;
} dict_str_str;

typedef struct _dict_str_loc {
    char *key;
    u64 value;
    struct _dict_str_loc *next;
} dict_str_loc;

int str_int_add(char *key, int value);
int str_int_find(char *key, int *out);
void str_int_clear();
void str_int_printall();
/*
int str_jmp_add(char *key, u64 value);
int str_jmp_find(char *key, u64 *out);
void str_jmp_clear();
*/
int str_str_add(char *key, char *value);
int str_str_find(char *key, char **out);
int str_str_index(int index, char **out);
void str_str_clear();