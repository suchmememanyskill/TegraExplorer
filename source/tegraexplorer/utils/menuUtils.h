#pragma once
#include "../../utils/types.h"
#include "../common/types.h"

void mu_createObjects(int size, menu_entry **menu);
void mu_clearObjects(menu_entry **menu);
int mu_countObjects(menu_entry *entries, u32 count, u8 propertyMask);
void mu_copySingle(char *name, u32 storage, u8 property, menu_entry *out);