#include "saveClass.h"
#include "compat.h"
#include "intClass.h"
#include "dictionaryClass.h"

u8 oneStringArgSave[] = {StringClass};
u8 oneStrOneByteArrayArgSave[] = {StringClass, ByteArrayClass};

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

ClassFunction(writeFile){
    Variable_t *arg = (*args);
    save_data_file_ctx_t dataArc;
    if (!save_open_file(&caller->save->saveCtx, &dataArc, arg->string.value, OPEN_MODE_WRITE))
        return newIntVariablePtr(1);

    u64 outBytes = 0;
    if (!save_data_file_write(&dataArc, &outBytes, 0, args[1]->solvedArray.vector.data, args[1]->solvedArray.vector.count)){
        return newIntVariablePtr(3);
    };

    if (outBytes != args[1]->solvedArray.vector.count){
        return newIntVariablePtr(4);
    }

    return newIntVariablePtr(0);
}

ClassFunction(getFiles){
    Variable_t* resPtr = newIntVariablePtr(0);
	Variable_t ret = {.variableType = DictionaryClass, .dictionary.vector = newVec(sizeof(Dict_t), 4)};
	addVariableToDict(&ret, "result", resPtr);

    save_data_directory_ctx_t ctx;
    if (!save_open_directory(&caller->save->saveCtx, &ctx, "/", OPEN_DIR_MODE_ALL)){
        resPtr->integer.value = 1;
        return copyVariableToPtr(ret);
    }

    u64 entryCount = 0;
    if (!save_data_directory_get_entry_count(&ctx, &entryCount)){
        resPtr->integer.value = 2;
        return copyVariableToPtr(ret);
    }

    directory_entry_t* entries = malloc(sizeof(directory_entry_t) * entryCount);
    u64 entryCountDirRead = 0;
    if (!save_data_directory_read(&ctx, &entryCountDirRead, entries, entryCount)){
        resPtr->integer.value = 3;

        return copyVariableToPtr(ret);
    }

	Variable_t fileNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t dirNamesArray = {.variableType = StringArrayClass, .solvedArray.vector = newVec(sizeof(char*), 0)};
	Variable_t fileSizeArray = {.variableType = IntArrayClass, .solvedArray.vector = newVec(sizeof(s64), 0)};

    for (int i = 0; i < entryCountDirRead; i++){
        char *add = CpyStr(entries[i].name);
        if (entries[i].type == DIR_ENT_TYPE_FILE){
            vecAdd(&fileNamesArray.solvedArray.vector, add);
            s64 fileSize = entries[i].size;
            vecAdd(&fileSizeArray.solvedArray.vector, fileSize);
        }
        else {
            vecAdd(&dirNamesArray.solvedArray.vector, add);
        }
    }

    free(entries);

	addVariableToDict(&ret, "files", copyVariableToPtr(fileNamesArray));
	addVariableToDict(&ret, "folders", copyVariableToPtr(dirNamesArray));
	addVariableToDict(&ret, "fileSizes", copyVariableToPtr(fileSizeArray));
	
	return copyVariableToPtr(ret);
}

ClassFunction(saveClassCommit){
    return newIntVariablePtr(!save_commit(&caller->save->saveCtx));
}

ClassFunctionTableEntry_t saveFunctions[] = {
    {"read", readFile, 1, oneStringArgSave},
    {"write", writeFile, 2, oneStrOneByteArrayArgSave},
    //{"readdir", getFiles, 1, oneStringArgSave}, // Seems broken?
    {"commit", saveClassCommit, 0, 0},
};

Variable_t getSaveMember(Variable_t* var, char* memberName) {
	return getGenericFunctionMember(var, memberName, saveFunctions, ARRAY_SIZE(saveFunctions));
}