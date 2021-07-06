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
#include <sec/se.h>

static const pkg1_id_t _pkg1_ids[] = {
	{ "20161121", 0 }, //1.0.0
	{ "20170210", 0 }, //2.0.0 - 2.3.0
	{ "20170519", 1 }, //3.0.0
	{ "20170710", 2 }, //3.0.1 - 3.0.2
	{ "20170921", 3 }, //4.0.0 - 4.1.0
	{ "20180220", 4 }, //5.0.0 - 5.1.0
	{ "20180802", 5 }, //6.0.0 - 6.1.0
	{ "20181107", 6 }, //6.2.0
	{ "20181218", 7 }, //7.0.0
	{ "20190208", 7 }, //7.0.1
	{ "20190314", 7 }, //8.0.0 - 8.0.1
	{ "20190531", 8 }, //8.1.0 - 8.1.1
	{ "20190809", 9 }, //9.0.0 - 9.0.1
	{ "20191021", 10}, //9.1.0 - 9.2.0
	{ "20200303", 10}, //10.0.0 - 10.2.0
	{ "20201030", 10}, //11.0.0 - 11.0.1
	{ "20210129", 10}, //12.0.0 - 12.0.1
	{ "20210422", 10}, //12.0.2 - 12.0.3
	{ "20210607", 11}, //12.1.0
	{ NULL } //End.
};

#define KB_FIRMWARE_VERSION_MAX 11

const pkg1_id_t *pkg1_identify(u8 *pkg1)
{
	for (u32 i = 0; _pkg1_ids[i].id; i++)
		if (!memcmp(pkg1 + 0x10, _pkg1_ids[i].id, 8))
			return &_pkg1_ids[i];

	char build_date[15];
	memcpy(build_date, (char *)(pkg1 + 0x10), 14);
	build_date[14] = 0;

	if (*(pkg1 + 0xE) != KB_FIRMWARE_VERSION_MAX + 1) {
		return NULL;
	}

	return &_pkg1_ids[ARRAY_SIZE(_pkg1_ids)-1];
}
