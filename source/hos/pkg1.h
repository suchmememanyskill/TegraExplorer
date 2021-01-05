/*
 * Copyright (c) 2018 naehrwert
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

#ifndef _PKG1_H_
#define _PKG1_H_

#include <utils/types.h>

#define PKG1_MAX_SIZE  0x40000
#define PKG1_OFFSET    0x100000
#define KEYBLOB_OFFSET 0x180000

typedef struct _bl_hdr_t210b01_t
{
	u8  aes_mac[0x10];
	u8  rsa_sig[0x100];
	u8  salt[0x20];
	u8  sha256[0x20];
	u32 version;
	u32 size;
	u32 load_addr;
	u32 entrypoint;
	u8  rsvd[0x10];
} bl_hdr_t210b01_t;

typedef struct _pkg1_id_t
{
	const char *id;
	u32 kb;
} pkg1_id_t;

const pkg1_id_t *pkg1_identify(u8 *pkg1);

#endif
