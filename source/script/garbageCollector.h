#include "model.h"

void initGarbageCollector();
//void addPendingReference(Variable_t* ref);
void processPendingReferences();
void exitGarbageCollector();
//void removePendingReference(Variable_t* ref);

void modReference(Variable_t* ref, u8 add);

#define removePendingReference(ref) modReference(ref, 0)
#define addPendingReference(ref) modReference(ref, 1)