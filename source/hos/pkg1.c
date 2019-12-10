/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 st4rk
 * Copyright (c) 2018-2019 CTCaer
 * Copyright (c) 2018 balika011
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

#include "pkg1.h"
#include "../sec/se.h"

#define HASH_ORDER_100_100 {2, 3, 4, 0, 5, 6, 1}
#define HASH_ORDER_200_510 {2, 3, 4, 0, 5, 7, 10, 12, 11, 6, 8, 1}
#define HASH_ORDER_600_620 {6, 5, 10, 7, 8, 2, 3, 4, 0, 12, 11, 1}
#define HASH_ORDER_700_9xx {6, 5, 10, 7, 8, 2, 3, 4, 0, 12, 11, 9, 1}

static const pkg1_id_t _pkg1_ids[] = {
	{ "20161121183008", 0, {0x1b517, 0x125bc2, 1, 16,  6, HASH_ORDER_100_100,      0, 0x449dc} }, //1.0.0
	{ "20170210155124", 0, {0x1d226,   0x26fe, 0, 16, 11, HASH_ORDER_200_510, 0x557b, 0x3d41a} }, //2.0.0 - 2.3.0
	{ "20170519101410", 1, {0x1ffa6,   0x298b, 0, 16, 11, HASH_ORDER_200_510, 0x552d, 0x3cb81} }, //3.0.0
	{ "20170710161758", 2, {0x20026,   0x29ab, 0, 16, 11, HASH_ORDER_200_510, 0x552d, 0x3cb81} }, //3.0.1 - 3.0.2
	{ "20170921172629", 3, {0x1c64c,   0x37eb, 0, 16, 11, HASH_ORDER_200_510, 0x5382, 0x3711c} }, //4.0.0 - 4.1.0
	{ "20180220163747", 4, {0x1f3b4,   0x465b, 0, 16, 11, HASH_ORDER_200_510, 0x5a63, 0x37901} }, //5.0.0 - 5.1.0
	{ "20180802162753", 5, {0x27350,  0x17ff5, 1,  8, 11, HASH_ORDER_600_620, 0x5674, 0x1d5be} }, //6.0.0 - 6.1.0
	{ "20181107105733", 6, {0x27350,  0x17ff5, 1,  8, 11, HASH_ORDER_600_620, 0x5674, 0x1d5be} }, //6.2.0
	{ "20181218175730", 7, {0x29c50,   0x6a73, 0,  8, 12, HASH_ORDER_700_9xx, 0x5563, 0x1d437} }, //7.0.0
	{ "20190208150037", 7, {0x29c50,   0x6a73, 0,  8, 12, HASH_ORDER_700_9xx, 0x5563, 0x1d437} }, //7.0.1
	{ "20190314172056", 7, {0x29c50,   0x6a73, 0,  8, 12, HASH_ORDER_700_9xx, 0x5563, 0x1d437} }, //8.0.0 - 8.0.1
	{ "20190531152432", 8, {0x29c50,   0x6a73, 0,  8, 12, HASH_ORDER_700_9xx, 0x5563, 0x1d437} }, //8.1.0
	{ "20190809135709", 9, {0x2ec10,   0x5573, 0,  1, 12, HASH_ORDER_700_9xx, 0x6495, 0x1d807} }, //9.0.0 - 9.0.1
	{ "20191021113848", 10,{0x2ec10,   0x5573, 0,  1, 12, HASH_ORDER_700_9xx, 0x6495, 0x1d807} }, //9.1.0
	{ NULL } //End.
};

const pkg1_id_t *pkg1_identify(u8 *pkg1)
{
	for (u32 i = 0; _pkg1_ids[i].id; i++)
		if (!memcmp(pkg1 + 0x10, _pkg1_ids[i].id, 12))
			return &_pkg1_ids[i];
	return NULL;
}
