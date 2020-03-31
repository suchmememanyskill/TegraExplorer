#pragma once
#include "../../utils/types.h"

typedef void (*func_void_ptr)();
typedef int (*func_int_ptr)();

typedef struct {
    char *key;
    func_int_ptr value;
    u8 arg_count;
} str_fnc_struct;

int run_function(char *func_name, int *out);