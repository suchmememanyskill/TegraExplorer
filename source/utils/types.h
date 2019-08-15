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

#ifndef _TYPES_H_
#define _TYPES_H_

#define NULL ((void *)0)

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define OFFSET_OF(t, m) ((u32)&((t *)NULL)->m)
#define CONTAINER_OF(mp, t, mn) ((t *)((u32)mp - OFFSET_OF(t, mn)))

#define KB_FIRMWARE_VERSION_100_200 0
#define KB_FIRMWARE_VERSION_300 1
#define KB_FIRMWARE_VERSION_301 2
#define KB_FIRMWARE_VERSION_400 3
#define KB_FIRMWARE_VERSION_500 4
#define KB_FIRMWARE_VERSION_600 5
#define KB_FIRMWARE_VERSION_620 6
#define KB_FIRMWARE_VERSION_700 7
#define KB_FIRMWARE_VERSION_810 8
#define KB_FIRMWARE_VERSION_MAX KB_FIRMWARE_VERSION_810

#define HOS_PKG11_MAGIC 0x31314B50

#define COLOR_RED    0xFFE70000
#define COLOR_ORANGE 0xFFFF8C00
#define COLOR_YELLOW 0xFFFFFF40
#define COLOR_GREEN  0xFF40FF00
#define COLOR_BLUE   0xFF00DDFF
#define COLOR_VIOLET 0xFF8040FF
#define COLOR_DEFAULT 0xFF1B1B1B
#define COLOR_WHITE 0xFFFFFFFF

typedef signed char s8;
typedef short s16;
typedef short SHORT;
typedef int s32;
typedef int INT;
typedef long LONG;
typedef long long int s64;
typedef unsigned char u8;
typedef unsigned char BYTE;
typedef unsigned short u16;
typedef unsigned short WORD;
typedef unsigned short WCHAR;
typedef unsigned int u32;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned long long QWORD;
typedef unsigned long long int u64;
typedef volatile unsigned char vu8;
typedef volatile unsigned short vu16;
typedef volatile unsigned int vu32;

typedef int bool;
#define true  1
#define false 0

#define BOOT_CFG_AUTOBOOT_EN (1 << 0)
#define BOOT_CFG_FROM_LAUNCH (1 << 1)
#define BOOT_CFG_SEPT_RUN    (1 << 7)

typedef struct __attribute__((__packed__)) _boot_cfg_t
{
	u8  boot_cfg;
	u8  autoboot;
	u8  autoboot_list;
	u8  extra_cfg;
	u32 sd_timeoff;
	u8  rsvd[124];
} boot_cfg_t;

typedef struct __attribute__((__packed__)) _reloc_meta_t
{
	u32 start;
	u32 stack;
	u32 end;
	u32 ep;
} reloc_meta_t;

#endif
