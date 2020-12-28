#pragma once
#include <utils/types.h>

typedef struct {
    void* data;
    u32 capacity; 
    u32 count;
    u32 elemSz;
    // u32 typeTag;
} Vector_t;

#define FREE(x) free(x); x = NULL;

#define vecAddElem(v, elem) vecAdd(v, &elem, sizeof(elem))
#define vecAddElement(v, elem) vecAdd(v, &elem, sizeof(elem))
#define vecDefArray(type, varName, vec) type varName = (type)((vec).data)
#define vecGetArray(type, vec) (type)((vec).data)
#define vecPDefArray(type, varName, vec) type varName = (type)((vec)->data)
#define vecPGetArray(type, vec) (type)((vec)->data)
#define vecFreePtr(vec) FREE(vec->data)
#define vecFree(vec) FREE(vec.data)
#define vecGetCapacity(vec) (vec.capacity / vec.elemSz)

#define vecGetArrayPtr(vec, type) (type)((vec)->data)

Vector_t newVec(u32 typesz, u32 preallocate);
Vector_t vecFromArray(void* array, u32 count, u32 typesz);
bool vecAdd(Vector_t* v, void* elem, u32 sz);
Vector_t vecCopy(Vector_t* orig);