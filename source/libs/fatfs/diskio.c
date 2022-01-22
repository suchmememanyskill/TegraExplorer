/*
 * Copyright (c) 2019-2020 shchmue
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

/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <string.h>
#include <bdk.h>
#include <libs/fatfs/diskio.h>	/* FatFs lower layer API */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv /* Physical drive number to identify the drive */
)
{
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
	BYTE pdrv /* Physical drive number to identify the drive */
)
{
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv)
	{
	case DRIVE_SD:
		return sdmmc_storage_read(&sd_storage, sector, count, buff) ? RES_OK : RES_ERROR;

	case DRIVE_BIS:
		return nx_emmc_bis_read(sector, count, buff);
	}

	return RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	switch (pdrv)
	{
	case DRIVE_SD:
		return sdmmc_storage_write(&sd_storage, sector, count, (void *)buff) ? RES_OK : RES_ERROR;

	case DRIVE_BIS:
		return nx_emmc_bis_write(sector, count, (void *)buff);
	}

	return RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
static u32 part_rsvd_size = 0;
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DWORD *buf = (DWORD *)buff;

	if (pdrv == DRIVE_SD)
	{
		switch (cmd)
		{
		case GET_SECTOR_COUNT:
			*buf = sd_storage.sec_cnt - part_rsvd_size;
			break;
		case GET_BLOCK_SIZE:
			*buf = 32768; // Align to 16MB.
			break;
		}
	}
	else if (pdrv == DRIVE_RAM)
	{
		switch (cmd)
		{
		case GET_SECTOR_COUNT:
			*buf = RAM_DISK_SZ >> 9; // 1GB.
			break;
		case GET_BLOCK_SIZE:
			*buf = 2048; // Align to 1MB.
			break;
		}
	}

	return RES_OK;
}
