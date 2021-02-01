#include "vector.h"
#include <string.h>
#include <mem/heap.h>

Vector_t newVec(u32 typesz, u32 preallocate)
{
    Vector_t res = {
        .data = calloc(preallocate, typesz),
        .capacity = preallocate * typesz,
        .count = 0,
        .elemSz = typesz
    };
    // check .data != null;
    return res;
}

Vector_t vecFromArray(void* array, u32 count, u32 typesz)
{
    Vector_t res = {
        .data = array,
        .capacity = count * typesz,
        .count = count,
        .elemSz = typesz
    };
    return res;
}

bool vecAdd(Vector_t* v, void* elem, u32 sz)
{
    if (!v || !elem || v->elemSz != sz)
        return false;

    u32 usedbytes = v->count * sz;
    if (usedbytes >= v->capacity)
    {
        v->capacity *= 2;
        void *buff = malloc(v->capacity);
        if (!buff)
            return false;
        memcpy(buff, v->data, v->capacity / 2);
        free(v->data);
        v->data = buff;
    }

    memcpy((char*)v->data + usedbytes, elem, sz);
    v->count++;
    return true;
}

Vector_t vecCopyOffset(Vector_t* orig, u32 offset) {
	Vector_t dst = newVec(orig->elemSz, orig->count - offset);
	memcpy(dst.data, ((u8*)orig->data + orig->elemSz * offset), (orig->count - offset) * orig->elemSz);
	dst.count = orig->count - offset;
	return dst;
}

Vector_t vecCopy(Vector_t* orig) {
    return vecCopyOffset(orig, 0);
}