/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2019 CTCaer
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "pkg2.h"
#include "../utils/aarch64_util.h"
#include "../mem/heap.h"
#include "../sec/se.h"
#include "../libs/compr/blz.h"

#include "../gfx/gfx.h"

/*#include "util.h"
#define DPRINTF(...) gfx_printf(__VA_ARGS__)
#define DEBUG_PRINTING*/
#define DPRINTF(...)

static u32 _pkg2_calc_kip1_size(pkg2_kip1_t *kip1)
{
	u32 size = sizeof(pkg2_kip1_t);
	for (u32 j = 0; j < KIP1_NUM_SECTIONS; j++)
		size += kip1->sections[j].size_comp;
	return size;
}

void pkg2_get_newkern_info(u8 *kern_data)
{
	u32 info_op = *(u32 *)(kern_data + PKG2_NEWKERN_GET_INI1);
	pkg2_newkern_ini1_val = ((info_op & 0xFFFF) >> 3) + PKG2_NEWKERN_GET_INI1; // Parse ADR and PC.

	pkg2_newkern_ini1_start = *(u32 *)(kern_data + pkg2_newkern_ini1_val);
	pkg2_newkern_ini1_end   = *(u32 *)(kern_data + pkg2_newkern_ini1_val + 0x8);
}

void pkg2_parse_kips(link_t *info, pkg2_hdr_t *pkg2, bool *new_pkg2)
{
	u8 *ptr;
	// Check for new pkg2 type.
	if (!pkg2->sec_size[PKG2_SEC_INI1])
	{
		pkg2_get_newkern_info(pkg2->data);

		ptr = pkg2->data + pkg2_newkern_ini1_start;
		*new_pkg2 = true;
	}
	else
		ptr = pkg2->data + pkg2->sec_size[PKG2_SEC_KERNEL];

	pkg2_ini1_t *ini1 = (pkg2_ini1_t *)ptr;
	ptr += sizeof(pkg2_ini1_t);

	for (u32 i = 0; i < ini1->num_procs; i++)
	{
		pkg2_kip1_t *kip1 = (pkg2_kip1_t *)ptr;
		pkg2_kip1_info_t *ki = (pkg2_kip1_info_t *)malloc(sizeof(pkg2_kip1_info_t));
		ki->kip1 = kip1;
		ki->size = _pkg2_calc_kip1_size(kip1);
		list_append(info, &ki->link);
		ptr += ki->size;
DPRINTF(" kip1 %d:%s @ %08X (%08X)\n", i, kip1->name, (u32)kip1, ki->size);
	}
}

int pkg2_decompress_kip(pkg2_kip1_info_t* ki, u32 sectsToDecomp)
{
	u32 compClearMask = ~sectsToDecomp;
	if ((ki->kip1->flags & compClearMask) == ki->kip1->flags)
		return 0; // Already decompressed, nothing to do.

	pkg2_kip1_t hdr;
	memcpy(&hdr, ki->kip1, sizeof(hdr));

	unsigned int newKipSize = sizeof(hdr);
	for (u32 sectIdx = 0; sectIdx < KIP1_NUM_SECTIONS; sectIdx++)
	{
		u32 sectCompBit = 1u << sectIdx;
		// For compressed, cant get actual decompressed size without doing it, so use safe "output size".
		if (sectIdx < 3 && (sectsToDecomp & sectCompBit) && (hdr.flags & sectCompBit))
			newKipSize += hdr.sections[sectIdx].size_decomp;
		else
			newKipSize += hdr.sections[sectIdx].size_comp;
	}

	pkg2_kip1_t* newKip = malloc(newKipSize);
	unsigned char* dstDataPtr = newKip->data;
	const unsigned char* srcDataPtr = ki->kip1->data;
	for (u32 sectIdx = 0; sectIdx < KIP1_NUM_SECTIONS; sectIdx++)
	{
		u32 sectCompBit = 1u << sectIdx;
		// Easy copy path for uncompressed or ones we dont want to uncompress.
		if (sectIdx >= 3 || !(sectsToDecomp & sectCompBit) || !(hdr.flags & sectCompBit))
		{
			unsigned int dataSize = hdr.sections[sectIdx].size_comp;
			if (dataSize == 0)
				continue;

			memcpy(dstDataPtr, srcDataPtr, dataSize);
			srcDataPtr += dataSize;
			dstDataPtr += dataSize;
			continue;
		}

		unsigned int compSize = hdr.sections[sectIdx].size_comp;
		unsigned int outputSize = hdr.sections[sectIdx].size_decomp;
		//gfx_printf("Decomping %s KIP1 sect %d of size %d...\n", (const char*)hdr.name, sectIdx, compSize);
		if (blz_uncompress_srcdest(srcDataPtr, compSize, dstDataPtr, outputSize) == 0)
		{
			gfx_printf("%kERROR decomping sect %d of %s KIP!%k\n", 0xFFFF0000, sectIdx, (char*)hdr.name, 0xFFCCCCCC);
			free(newKip);

			return 1;
		}
		else
		{
			DPRINTF("Done! Decompressed size is %d!\n", outputSize);
		}
		hdr.sections[sectIdx].size_comp = outputSize;
		srcDataPtr += compSize;
		dstDataPtr += outputSize;
	}

	hdr.flags &= compClearMask;
	memcpy(newKip, &hdr, sizeof(hdr));
	newKipSize = dstDataPtr-(unsigned char*)(newKip);

	free(ki->kip1);
	ki->kip1 = newKip;
	ki->size = newKipSize;

	return 0;
}

pkg2_hdr_t *pkg2_decrypt(void *data)
{
	u8 *pdata = (u8 *)data;

	// Skip signature.
	pdata += 0x100;

	pkg2_hdr_t *hdr = (pkg2_hdr_t *)pdata;

	// Skip header.
	pdata += sizeof(pkg2_hdr_t);

	// Decrypt header.
	se_aes_crypt_ctr(8, hdr, sizeof(pkg2_hdr_t), hdr, sizeof(pkg2_hdr_t), hdr);
	//gfx_hexdump((u32)hdr, hdr, 0x100);

	if (hdr->magic != PKG2_MAGIC)
		return NULL;

	for (u32 i = 0; i < 4; i++)
	{
DPRINTF("sec %d has size %08X\n", i, hdr->sec_size[i]);
		if (!hdr->sec_size[i])
			continue;

		se_aes_crypt_ctr(8, pdata, hdr->sec_size[i], pdata, hdr->sec_size[i], &hdr->sec_ctr[i * 0x10]);
		//gfx_hexdump((u32)pdata, pdata, 0x100);

		pdata += hdr->sec_size[i];
	}

	return hdr;
}
