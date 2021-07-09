#ifdef WIN32
#pragma once

#include "model.h"

int vecAddElem(Vector_t* v, void* elem, u8 sz);
Vector_t newVec(u8 typesz, u32 preallocate);
Vector_t vecCopy(Vector_t* orig);
Vector_t vecCopyOffset(Vector_t* orig, u32 offset);
Vector_t vecFromArray(void* array, u32 count, u32 typesz);

#define vecAdd(vec, element) vecAddElem(vec, &element, sizeof(element))
#define vecGetArrayPtr(vec, type) (type)(vec)->data
#define vecGetArray(vec, type) (type)(vec).data
#define vecFreePtr(vec) FREE(vec->data)
#define vecFree(vec) FREE(vec.data)
#define vecGetCapacity(vec) (vec.capacity / vec.elemSz)

#define vecForEach(type, varname, vecPtr) for (type varname = vecPtr->data; ((u8*)varname - (u8*)vecPtr->data) < (vecPtr->count * vecPtr->elemSz); varname++)

void* getStackEntry(Vector_t* stack);
void* popStackEntry(Vector_t* stack);
void vecRem(Vector_t * vec, int idx);
#endif