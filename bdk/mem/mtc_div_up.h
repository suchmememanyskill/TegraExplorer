#include <stdint.h>
#include <assert.h>

static inline uint32_t UtilDivideUp(uint32_t v, uint32_t d) {
    assert(d > 0);

    uint32_t add = d - 1;

    return (uint32_t)(((uint64_t)v + (uint64_t)add) / (uint64_t)d);
}
