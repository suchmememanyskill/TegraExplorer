#include "model.h"
#include "genericClass.h"
#include "compat.h"
#include "garbageCollector.h"

typedef struct {
	Variable_t* ref;
	u16 refCount;
} ReferenceCounter_t;

Vector_t pendingAdd = { 0 };
Vector_t pendingRemove = { 0 };
Vector_t storedReferences = { 0 };

void initGarbageCollector() {
	pendingAdd = newVec(sizeof(Variable_t*), 4);
	pendingRemove = newVec(sizeof(Variable_t*), 4);
	storedReferences = newVec(sizeof(ReferenceCounter_t), 8);
}

// TODO: create binary tree for this!

void modReference(Variable_t* ref, u8 add) {
	if (ref == NULL || ref->gcDoNotFree)
		return;

	ReferenceCounter_t* additionalFree = NULL;

	vecForEach(ReferenceCounter_t*, references, (&storedReferences)) {
		if (!add && (
			(ref->variableType == FunctionClass && ref->function.builtIn && references->ref == ref->function.origin) || 
			(ref->variableType == SolvedArrayReferenceClass && references->ref == ref->solvedArray.arrayClassReference)))
			if (--references->refCount <= 0) {
				additionalFree = references;
				continue;
			}

		if (references->ref == ref) {
			if (add)
				references->refCount++;
			else {
				if (--references->refCount <= 0) {
					freeVariable(&references->ref);
					vecRem(&storedReferences, ((u8*)references - (u8*)storedReferences.data) / storedReferences.elemSz);

					if (additionalFree != NULL) {
						freeVariable(&additionalFree->ref);
						vecRem(&storedReferences, ((u8*)additionalFree - (u8*)storedReferences.data) / storedReferences.elemSz);
					}
				}
			}

			return;
		}
	}
	
	if (!add)
		return;

	ReferenceCounter_t r = { .ref = ref, .refCount = 1 };
	vecAdd(&storedReferences, r);
}
/*
void addPendingReference(Variable_t* ref) {
	if (ref == NULL || ref->gcDoNotFree)
		return;

	//vecAdd(&pendingAdd, ref);

	modReference(ref, 1);

	// TODO: freeing issues when trying a = 0 while(1) { a.print() 1.print() a = a + 1 } 

	//if (pendingAdd.count >= 16)
	//	processPendingReferences();
}

void removePendingReference(Variable_t* ref) {
	if (ref == NULL || ref->gcDoNotFree)
		return;

	if (!ref->gcDoNotFree) {
		if (ref->variableType == FunctionClass && ref->function.builtIn) {
			removePendingReference(ref->function.origin);
		}

		if (ref->variableType == SolvedArrayReferenceClass) {
			removePendingReference(ref->solvedArray.arrayClassReference);
		}

		modReference(ref, 0);

		//vecAdd(&pendingRemove, ref);
	}
}
*/

void processPendingReferences() {
	return; //stubbed
	vecForEach(Variable_t**, references, (&pendingAdd))
		modReference(*references, 1);

	pendingAdd.count = 0;

	vecForEach(Variable_t**, references, (&pendingRemove))
		modReference(*references, 0);

	pendingRemove.count = 0;
}

void exitGarbageCollector() {
	processPendingReferences();

	vecForEach(ReferenceCounter_t*, references, (&storedReferences)) {
		gfx_printf("[WARN] referenced var %p at exit\n", references->ref);
		freeVariable(&references->ref);
	}

	vecFree(pendingAdd);
	vecFree(pendingRemove);
	vecFree(storedReferences);
}