#pragma once
#include <utils/types.h>
#include "../../utils/vector.h"
#include "../fstypes.h"

void clearFileVector(Vector_t *v);
Vector_t /* of type FSEntry_t */ ReadFolder(const char *path, int *res);