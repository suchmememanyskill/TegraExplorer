#pragma once
#include <utils/types.h>

typedef struct {
    void* data;
    u32 capacity; 
    u32 count;
    u8 elemSz;
    // u32 typeTag;
} Vector_t;

#define FREE(x) free(x); x = NULL;

#define vecAddElem(v, elem) _vecAdd(v, &elem, sizeof(elem))
#define vecAddElement(v, elem) _vecAdd(v, &elem, sizeof(elem))
#define vecAdd(vec, element) _vecAdd(vec, &element, sizeof(element))
#define vecDefArray(type, varName, vec) type varName = (type)((vec).data)
#define vecGetArray(type, vec) (type)((vec).data)
#define vecPDefArray(type, varName, vec) type varName = (type)((vec)->data)
#define vecPGetArray(type, vec) (type)((vec)->data)
#define vecFreePtr(vec) FREE(vec->data)
#define vecFree(vec) FREE(vec.data)
#define vecGetCapacity(vec) (vec.capacity / vec.elemSz)

#define vecGetArrayPtr(vec, type) (type)((vec)->data)

#define vecForEach(type, varname, vecPtr) for (type varname = vecPtr->data; ((u8*)varname - (u8*)vecPtr->data) < (vecPtr->count * vecPtr->elemSz); varname++)

Vector_t newVec(u8 typesz, u32 preallocate);
Vector_t vecFromArray(void* array, u32 count, u32 typesz);
bool _vecAdd(Vector_t* v, void* elem, u8 sz);
Vector_t vecCopy(Vector_t* orig);
Vector_t vecCopyOffset(Vector_t* orig, u32 offset);

void* getStackEntry(Vector_t* stack);
void* popStackEntry(Vector_t* stack);
void vecRem(Vector_t * vec, int idx);