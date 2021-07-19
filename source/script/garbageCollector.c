#include "model.h"
#include "genericClass.h"
#include "compat.h"
#include "garbageCollector.h"

void modReference(Variable_t* ref, u8 add) {
	if (ref == NULL || ref->gcDoNotFree)
		return;

	if (add) {
		ref->tagCount++;
	}
	else {
		ref->tagCount--;
		if (ref->tagCount <= 0) {
			// TODO: move to parser.c
			if (ref->variableType == FunctionClass && ref->function.builtIn && ref->function.origin != NULL)
				modReference(ref->function.origin, 0);

			if (ref->variableType == SolvedArrayReferenceClass)
				modReference(ref->solvedArray.arrayClassReference, 0);

			freeVariable(&ref);
		}
	}
}