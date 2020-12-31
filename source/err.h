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
    TE_ERR_KEYDUMP_FAIL,
    TE_ERR_PARTITION_NOT_FOUND,
    TE_ERR_PATH_IN_PATH,
    TE_ERR_EMMC_READ_FAIL,
    TE_ERR_EMMC_WRITE_FAIL,
    TE_ERR_NO_SD,
    TE_ERR_FILE_TOO_BIG_FOR_DEST,
    TE_ERR_MEM_ALLOC_FAIL,
    TE_WARN_FILE_EXISTS,
    TE_WARN_FILE_TOO_SMALL_FOR_DEST,
};

#define newErrCode(err) (ErrCode_t) {err, __LINE__, __FILE__}
void DrawError(ErrCode_t err);