#pragma once
#include <utils/types.h>

typedef struct _bmp_t
{
	u16 magic;
	u32 size;
	u32 rsvd;
	u32 data_off;
	u32 hdr_size;
	u32 width;
	u32 height;
	u16 planes;
	u16 pxl_bits;
	u32 comp;
	u32 img_size;
	u32 res_h;
	u32 res_v;
	u64 rsvd2;
} __attribute__((packed)) bmp_t;

void FormatSD();
void TakeScreenshot();