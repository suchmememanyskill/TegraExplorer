#include "menuUtils.h"
#include "../../utils/types.h"
#include "../common/types.h"
#include "../../mem/heap.h"
#include "utils.h"

void mu_clearObjects(menu_entry **menu){
    if ((*menu) != NULL){
        for (int i = 0; (*menu)[i].name != NULL; i++){
            free((*menu)[i].name);
            (*menu)[i].name = NULL;
        }
        free((*menu));
        (*menu) = NULL;
    }
}

void mu_createObjects(int size, menu_entry **menu){
    (*menu) = calloc (size + 1, sizeof(menu_entry));
    (*menu)[size].name = NULL;
}

int mu_countObjects(menu_entry *entries, u8 propertyMask){
    int amount = 0;

    for (u32 i = 0; entries[i].name != NULL; i++){
        if (!(entries[i].property & propertyMask))
            amount++;
    }

    while (entries[amount].name != NULL)
        amount++;

    return amount;
}

void mu_copySingle(char *name, u32 storage, u8 property, menu_entry *out){
    if (out->name != NULL)
        free(out->name);

    utils_copystring(name, &out->name);
    out->storage = storage;
    out->property = property;
}