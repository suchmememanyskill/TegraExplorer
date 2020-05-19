/*
* Copyright (c) 2018 naehrwert
* Copyright (c) 2018 CTCaer
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

#ifndef _TSEC_H_
#define _TSEC_H_

#include "../utils/types.h"

#define TSEC_KEY_DATA_ADDR 0x300

typedef struct _tsec_ctxt_t
{
	void *fw;
	u32 size;
	void *pkg1;
} tsec_ctxt_t;

typedef struct _tsec_key_data_t
{
	u8 debug_key[0x10];
	u8 blob0_auth_hash[0x10];
	u8 blob1_auth_hash[0x10];
	u8 blob2_auth_hash[0x10];
	u8 blob2_aes_iv[0x10];
	u8 hovi_eks_seed[0x10];
	u8 hovi_common_seed[0x10];
	u32 blob0_size;
	u32 blob1_size;
	u32 blob2_size;
	u32 blob3_size;
	u32 blob4_size;
} tsec_key_data_t;

int tsec_query(u8 *tsec_keys, u8 kb, tsec_ctxt_t *tsec_ctxt);

#endif
