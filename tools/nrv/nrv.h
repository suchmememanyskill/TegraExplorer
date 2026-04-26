#include "types.h"

#ifndef NRV_H
#define NRV_H

int nrv2e_99_compress(const byte *in, u32 in_len,
                         byte *out, u32 *out_len,
                         // ucl_progress_callback_p cb,
                         int level,
                         const struct compress_config_t *conf,
                         u32 *result);
#endif
