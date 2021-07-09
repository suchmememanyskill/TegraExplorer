#ifdef WIN32

#include <stdio.h>
#include "compat.h"
#include "parser.h"
#include "intClass.h"
#include "StringClass.h"
#include "eval.h"
#include "garbageCollector.h"

// TODO: error handling DONE
// TODO: unsolved arrays
// TODO: free-ing vars & script at end
// TODO: implement functions from tsv2
// TODO: add len to string
// TODO: clear old int values from array DONE
// TODO: int and str should be statically included in OP_t DONE

char* readFile(char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0L, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    char* ret = calloc(size + 1, 1);
    fread(ret, size, 1, fp);

    fclose(fp);
    return ret;
}

int main()
{
    //gfx_printf("Hello world!\n");

    //StartParse("a b c de 0x1 0x10 \"yeet\" + + ");
    /*
    Variable_t b = newStringVariable("Hello world\n", 1, 0);
    callClass("__print__", &b, NULL, NULL);

    Variable_t a = newIntVariable(69, 0);
    Variable_t c = newIntVariable(1, 0);
    Variable_t e = newStringVariable("snake", 1, 0);
    Vector_t fuk = newVec(sizeof(Variable_t), 1);
    vecAdd(&fuk, c);
    Variable_t *d = callClass("+", &a, NULL, &fuk);
    callClass("__print__", d, NULL, NULL);
    */
    /*
    Vector_t v = newVec(sizeof(int), 4);
    int a = 69;
    vecAdd(&v, a);
    vecAdd(&v, a);
    vecAdd(&v, a);
    vecAdd(&v, a);

    vecForEach(int*, b, (&v))
        printf("%d\n", *b);

    return;
    */
    
    initGarbageCollector();

    char* script = readFile("input.te");
    if (script == NULL)
        return;

    //parseScript("#REQUIRE VER 3.0.5\nmain = { two = 1 + 1 }");
    //ParserRet_t ret = parseScript("a.b.c(1){ a.b.c() }");
    ParserRet_t ret = parseScript(script);
    free(script);

    setStaticVars(&ret.staticVarHolder);
    initRuntimeVars();
    
    Variable_t* res = eval(ret.main.operations.data, ret.main.operations.count, 1);

    exitRuntimeVars();
    exitGarbageCollector();
    exitStaticVars(&ret.staticVarHolder);
    exitFunction(ret.main.operations.data, ret.main.operations.count);
    vecFree(ret.staticVarHolder);
    vecFree(ret.main.operations);

    gfx_printf("done");
}
#endif