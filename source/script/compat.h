#pragma once

#ifdef WIN32
	#include <stdio.h>
	#include <malloc.h>
#define gfx_printf(str, ...) printf(str, ##__VA_ARGS__)
	#define gfx_vprintf(str, va) vprintf(str, va);
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
	#define LP_VER_MJ 3
	#define LP_VER_MN 0
	#define LP_VER_BF 5
	#define FREE(x) if (x) free(x)
	#define CpyStr(x) _strdup(x);
	#include "vector.h"
	#pragma _CRT_SECURE_NO_WARNINGS
#else
	#include "../gfx/gfx.h"
	#include <mem/heap.h>
	#include "../utils/vector.h"
	#include "../utils/utils.h"
#endif