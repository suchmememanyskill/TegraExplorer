#include "vector.h"
#include <string.h>
#include <mem/heap.h>

Vector_t newVec(u8 typesz, u32 preallocate) {
	if (preallocate) {
		Vector_t res = {
			.data = calloc(preallocate, typesz),
			.capacity = preallocate * typesz,
			.count = 0,
			.elemSz = typesz
		};

		// check .data != null;
		return res;
	}
	else {
		Vector_t res = {
			.data = NULL,
			.capacity = 1 * typesz,
			.count = 0,
			.elemSz = typesz
		};

		return res;
	}
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

int vecAdd(Vector_t* v, void* elem, u8 sz) {
	if (!v || !elem || v->elemSz != sz)
		return 0;

	if (v->data == NULL) {
		v->data = calloc(1, v->elemSz);
	}

	u32 usedbytes = v->count * sz;
	if (usedbytes >= v->capacity)
	{
		v->capacity *= 2;
		void* buff = malloc(v->capacity);
		if (!buff)
			return 0;
		memcpy(buff, v->data, v->capacity / 2);
		free(v->data);
		v->data = buff;
	}

	memcpy((char*)v->data + usedbytes, elem, sz);
	v->count++;
	return 1;
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


void* getStackEntry(Vector_t *stack) {
	if (stack->count <= 0)
		return NULL;

	return ((u8*)stack->data + (stack->elemSz * (stack->count - 1)));
}

// This will stay valid until the queue is modified
void* popStackEntry(Vector_t* stack) {
	if (stack->count <= 0)
		return NULL;

	void* a = getStackEntry(stack);
	stack->count--;
	return a;
}

void vecRem(Vector_t *vec, int idx) {
	if (vec->count <= 0 || idx >= vec->count)
		return;

	if (idx == (vec->count - 1)) {
		vec->count--;
		return;
	}

	memcpy((u8*)vec->data + (vec->elemSz * idx), (u8*)vec->data + (vec->elemSz * (idx + 1)), (vec->count - idx - 1) * vec->elemSz);
	vec->count--;
}