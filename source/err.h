#pragma once
#include <utils/types.h>

typedef struct {
    u16 err;
    u16 loc;
    char* file;
} ErrCode_t;

enum {
    TE_ERR_UNIMPLEMENTED = 21,
};

#define newErrCode(err) (ErrCode_t) {err, __LINE__, __FILE__}
void DrawError(ErrCode_t err);