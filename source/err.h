#pragma once
#include <utils/types.h>

typedef struct {
    u16 err;
    u16 loc;
    char* file;
} ErrCode_t;

enum {
    TE_ERR_UNIMPLEMENTED = 15,
    TE_EXCEPTION_RESET,
    TE_EXCEPTION_UNDEFINED,
    TE_EXCEPTION_PREF_ABORT,
    TE_EXCEPTION_DATA_ABORT,
    TE_ERR_SAME_LOC,
    TE_ERR_KEYDUMP_FAIL
};

#define newErrCode(err) (ErrCode_t) {err, __LINE__, __FILE__}
void DrawError(ErrCode_t err);