#include "saveClass.h"
#include "compat.h"

u8 oneStringArgSave[] = {StringClass};

ClassFunction(readFile){
    Variable_t *arg = (*args);
    save_data_file_ctx_t dataArc;
    if (!save_open_file(&caller->save->saveCtx, &dataArc, arg->string.value, OPEN_MODE_READ))
        return NULL;
    
    u64 totalSize;
    save_data_file_get_size(&dataArc, &totalSize);

    u8 *buff = malloc(totalSize);

    save_data_file_read(&dataArc, &totalSize, 0, buff, totalSize);

    Variable_t a = {.variableType = ByteArrayClass};
    a.solvedArray.vector = vecFromArray(buff, totalSize, 1);
    return copyVariableToPtr(a);
}

ClassFunctionTableEntry_t saveFunctions[] = {
    {"readFile", readFile, 1, oneStringArgSave},
};

Variable_t getSaveMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, saveFunctions, ARRAY_SIZE(saveFunctions));
}