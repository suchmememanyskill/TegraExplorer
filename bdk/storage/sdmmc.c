/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2026 CTCaer
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

#include <mem/heap.h>
#include <mem/mc.h>
#include <soc/timer.h>
#include <storage/emmc.h>
#include <storage/sdmmc.h>
#include <storage/mmc_def.h>
#include <storage/sd.h>
#include <storage/sd_def.h>
#include <memory_map.h>
#include <gfx_utils.h>

//#define DPRINTF(...) gfx_printf(__VA_ARGS__)
#define DPRINTF(...)

//#define SDMMC_DEBUG_PRINT_SD_REGS
#ifdef SDMMC_DEBUG_PRINT_SD_REGS
#define DREGPRINTF(...) gfx_printf(__VA_ARGS__)
#else
#define DREGPRINTF(...)
#endif

#ifdef BDK_SDMMC_EXTRA_PRINT
#define ERROR_EXTRA_PRINTING
#endif

u32 sd_power_cycle_time_start;

static inline u32 unstuff_bits(const u32 *resp, u32 start, u32 size)
{
	start %= 128;

	const u32 mask = (size < 32 ? 1 << size : 0) - 1;
	const u32 off  = 3 - ((start) / 32);
	const u32 shft = (start) & 31;

	u32 res = resp[off] >> shft;
	if (size + shft > 32)
		res |= resp[off - 1] << ((32 - shft) % 32);
	return res & mask;
}

/*
 * Common functions for SD and MMC.
 */

static int _sdmmc_storage_check_card_status(u32 res)
{
	//Error mask:
	//!WARN: R1_SWITCH_ERROR is reserved on SD. The card isn't supposed to use it.
	if (res &
		(R1_OUT_OF_RANGE       | R1_ADDRESS_ERROR | R1_BLOCK_LEN_ERROR |
		 R1_ERASE_SEQ_ERROR    | R1_ERASE_PARAM   | R1_WP_VIOLATION    |
		 R1_LOCK_UNLOCK_FAILED | R1_COM_CRC_ERROR | R1_ILLEGAL_COMMAND |
		 R1_CARD_ECC_FAILED    | R1_CC_ERROR      | R1_ERROR           |
		 R1_CID_CSD_OVERWRITE  | R1_WP_ERASE_SKIP | R1_ERASE_RESET     |
		 R1_SWITCH_ERROR))
		return 1;

	// No errors.
	return 0;
}

static int _sdmmc_storage_check_cached_card_status(sdmmc_t *sdmmc)
{
	u32 resp;
	sdmmc_get_cached_rsp(sdmmc, &resp, SDMMC_RSP_TYPE_1);
	if (_sdmmc_storage_check_card_status(resp))
		return 1;

	return 0;
}

static int _sdmmc_storage_execute_cmd_ex_masked(sdmmc_storage_t *storage, u32 *resp, u32 cmd, u32 arg, u32 check_busy, u32 expected_state, u32 mask)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, cmd, arg, SDMMC_RSP_TYPE_1, check_busy);
	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
		return 1;

	sdmmc_get_cached_rsp(storage->sdmmc, resp, SDMMC_RSP_TYPE_1);
	if (mask)
		*resp &= ~mask;

	if (!_sdmmc_storage_check_card_status(*resp))
		if (expected_state == R1_SKIP_STATE_CHECK || R1_CURRENT_STATE(*resp) == expected_state)
			return 0;

	return 1;
}

static int _sdmmc_storage_execute_cmd_ex_state(sdmmc_storage_t *storage, u32 cmd, u32 arg, u32 check_busy, u32 expected_state)
{
	u32 tmp;
	return _sdmmc_storage_execute_cmd_ex_masked(storage, &tmp, cmd, arg, check_busy, expected_state, 0);
}

static int _sdmmc_storage_go_idle_state(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_GO_IDLE_STATE, 0, SDMMC_RSP_TYPE_0, 0);

	return sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL);
}

static int _sdmmc_storage_get_cid(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_ALL_SEND_CID, 0, SDMMC_RSP_TYPE_2, 0);
	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
		return 1;

	sdmmc_get_cached_rsp(storage->sdmmc, (u32 *)storage->raw_cid, SDMMC_RSP_TYPE_2);

	return 0;
}

static int _sdmmc_storage_select_card(sdmmc_storage_t *storage)
{
	return _sdmmc_storage_execute_cmd_ex_state(storage, MMC_SELECT_CARD, storage->rca << 16, 1, R1_SKIP_STATE_CHECK);
}

static int _sdmmc_storage_get_csd(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_SEND_CSD, storage->rca << 16, SDMMC_RSP_TYPE_2, 0);
	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
		return 1;

	sdmmc_get_cached_rsp(storage->sdmmc, (u32 *)storage->raw_csd, SDMMC_RSP_TYPE_2);

	return 0;
}

static int _sdmmc_storage_set_blocklen(sdmmc_storage_t *storage, u32 blocklen)
{
	return _sdmmc_storage_execute_cmd_ex_state(storage, MMC_SET_BLOCKLEN, blocklen, 0, R1_STATE_TRAN);
}

static int _sdmmc_storage_get_status(sdmmc_storage_t *storage, u32 *resp, u32 mask)
{
	return _sdmmc_storage_execute_cmd_ex_masked(storage, resp, MMC_SEND_STATUS, storage->rca << 16, 0, R1_STATE_TRAN, mask);
}

static int _sdmmc_storage_check_status(sdmmc_storage_t *storage)
{
	u32 tmp;
	return _sdmmc_storage_get_status(storage, &tmp, 0);
}

int sdmmc_storage_vendor_cmd(sdmmc_storage_t *storage, u32 arg)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_VENDOR_CMD_62, arg, SDMMC_RSP_TYPE_1, 1);
	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
		return 1;

	u32 resp;
	sdmmc_get_cached_rsp(storage->sdmmc, &resp, SDMMC_RSP_TYPE_1);

	resp = -1;
	u32 timeout = get_tmr_ms() + 1500;
	while (true)
	{
		_sdmmc_storage_get_status(storage, &resp, 0);

		if (resp == (R1_READY_FOR_DATA | R1_STATE(R1_STATE_TRAN)))
			break;

		if (get_tmr_ms() > timeout)
			break;
		msleep(10);
	}

	return _sdmmc_storage_check_card_status(resp);
}

int sdmmc_storage_vendor_sandisk_report(sdmmc_storage_t *storage, void *buf)
{
	// Request health report.
	if (sdmmc_storage_vendor_cmd(storage, MMC_SANDISK_HEALTH_REPORT))
		return 2;

	u32 tmp = 0;
	sdmmc_cmd_t cmdbuf;
	sdmmc_req_t reqbuf;

	sdmmc_init_cmd(&cmdbuf, MMC_VENDOR_CMD_63, 0, SDMMC_RSP_TYPE_1, 0); // similar to CMD17 with arg 0x0.

	reqbuf.buf              = buf;
	reqbuf.num_sectors      = 1;
	reqbuf.blksize          = SDMMC_DAT_BLOCKSIZE;
	reqbuf.is_write         = 0;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
	{
		_sdmmc_storage_get_status(storage, &tmp, 0);

		return 1;
	}

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

int sdmmc_storage_gen_cmd(sdmmc_storage_t *storage, u32 arg, void *buf)
{
	u32 tmp = 0;
	sdmmc_cmd_t cmdbuf;
	sdmmc_req_t reqbuf;

	sdmmc_init_cmd(&cmdbuf, MMC_GEN_CMD, arg, SDMMC_RSP_TYPE_1, 0);

	reqbuf.buf              = buf;
	reqbuf.blksize          = SDMMC_DAT_BLOCKSIZE;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = !(arg & 1);
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
	{
		_sdmmc_storage_get_status(storage, &tmp, 0);

		return 1;
	}

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static int _sdmmc_storage_readwrite_ex(sdmmc_storage_t *storage, u32 *blkcnt_out, u32 sector, u32 num_sectors, void *buf, u32 is_write)
{
	u32 tmp = 0;
	sdmmc_cmd_t cmdbuf;
	sdmmc_req_t reqbuf;

	// If SDSC convert block address to byte address.
	if (!storage->has_sector_access)
		sector <<= 9;

	sdmmc_init_cmd(&cmdbuf, is_write ? MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK, sector, SDMMC_RSP_TYPE_1, 0);

	reqbuf.buf              = buf;
	reqbuf.num_sectors      = num_sectors;
	reqbuf.blksize          = SDMMC_DAT_BLOCKSIZE;
	reqbuf.is_write         = is_write;
	reqbuf.is_multi_block   = 1;
	reqbuf.is_auto_stop_trn = 1;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, blkcnt_out))
	{
		sdmmc_stop_transmission(storage->sdmmc, &tmp);
		_sdmmc_storage_get_status(storage, &tmp, 0);

		return 1;
	}

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

int sdmmc_storage_end(sdmmc_storage_t *storage)
{
	DPRINTF("[SDMMC%d] end\n", storage->sdmmc->id);

	if (_sdmmc_storage_go_idle_state(storage))
		return 1;

	sdmmc_end(storage->sdmmc);

	storage->initialized = 0;

	return 0;
}

static int _sdmmc_storage_handle_io_error(sdmmc_storage_t *storage, bool first_reinit)
{
	int res = 1;

	if (storage->sdmmc->id == SDMMC_1 || storage->sdmmc->id == SDMMC_4)
	{
		if (storage->sdmmc->id == SDMMC_1)
		{
			sd_error_count_increment(SD_ERROR_RW_FAIL);

			if (first_reinit)
				res = sd_initialize(true);
			else
			{
				res = sd_init_retry(true);
				if (res)
					sd_error_count_increment(SD_ERROR_INIT_FAIL);
			}
		}
		else if (storage->sdmmc->id == SDMMC_4)
		{
			emmc_error_count_increment(EMMC_ERROR_RW_FAIL);

			if (first_reinit)
				res = emmc_initialize(true);
			else
			{
				res = emmc_init_retry(true);
				if (res)
					emmc_error_count_increment(EMMC_ERROR_INIT_FAIL);
			}
		}
	}

	return res;
}

static int _sdmmc_storage_readwrite(sdmmc_storage_t *storage, u32 sector, u32 num_sectors, void *buf, u32 is_write)
{
	u8 *bbuf = (u8 *)buf;
	u32 sct_off = sector;
	u32 sct_total = num_sectors;
	bool first_reinit = true;

	// Exit if not initialized.
	if (!storage->initialized)
		return 1;

	// Check if out of bounds.
	if (((u64)sector + num_sectors) > storage->sec_cnt)
	{
#ifdef ERROR_EXTRA_PRINTING
		EPRINTFARGS("SDMMC%d: Out of bounds!", storage->sdmmc->id + 1);
#endif
		return 1;
	}

	while (sct_total)
	{
		u32 blkcnt = 0;
		// Retry 5 times if failed.
		u32 retries = 5;
		do
		{
reinit_try:
			if (!_sdmmc_storage_readwrite_ex(storage, &blkcnt, sct_off, MIN(sct_total, SDMMC_AMAX_BLOCKNUM), bbuf, is_write))
				goto out;
			else
				retries--;

			sd_error_count_increment(SD_ERROR_RW_RETRY);

			msleep(50);
		} while (retries);

		// Disk IO failure! Reinit SD/EMMC to a lower speed.
		if (!_sdmmc_storage_handle_io_error(storage, first_reinit))
		{
			// Reset values for a retry.
			blkcnt = 0;
			retries = 3;
			first_reinit = false;

			bbuf = (u8 *)buf;
			sct_off = sector;
			sct_total = num_sectors;

			goto reinit_try;
		}

		// Failed.
		return 1;

out:
		sct_off += blkcnt;
		sct_total -= blkcnt;
		bbuf += SDMMC_DAT_BLOCKSIZE * blkcnt;
	}

	return 0;
}

int sdmmc_storage_read(sdmmc_storage_t *storage, u32 sector, u32 num_sectors, void *buf)
{
	// Ensure that SDMMC has access to buffer and it's SDMMC DMA aligned.
	if (mc_client_has_access(buf) && !((u32)buf % SDMMC_ADMA_ADDR_ALIGN))
		return _sdmmc_storage_readwrite(storage, sector, num_sectors, buf, 0);

	if (num_sectors > (SDMMC_ALT_DMA_BUF_SZ / SDMMC_DAT_BLOCKSIZE))
		return 1;

	u8 *tmp_buf = (u8 *)SDMMC_ALT_DMA_BUFFER;
	if (_sdmmc_storage_readwrite(storage, sector, num_sectors, tmp_buf, 0))
		return 1;

	memcpy(buf, tmp_buf, SDMMC_DAT_BLOCKSIZE * num_sectors);

	return 0;
}

int sdmmc_storage_write(sdmmc_storage_t *storage, u32 sector, u32 num_sectors, void *buf)
{
	// Ensure that SDMMC has access to buffer and it's SDMMC DMA aligned.
	if (mc_client_has_access(buf) && !((u32)buf % SDMMC_ADMA_ADDR_ALIGN))
		return _sdmmc_storage_readwrite(storage, sector, num_sectors, buf, 1);

	if (num_sectors > (SDMMC_ALT_DMA_BUF_SZ / SDMMC_DAT_BLOCKSIZE))
		return 1;

	u8 *tmp_buf = (u8 *)SDMMC_ALT_DMA_BUFFER;
	memcpy(tmp_buf, buf, SDMMC_DAT_BLOCKSIZE * num_sectors);

	return _sdmmc_storage_readwrite(storage, sector, num_sectors, tmp_buf, 1);
}

/*
* MMC specific functions.
*/

static int _mmc_storage_get_op_cond_inner(sdmmc_storage_t *storage, u32 *rocr, u32 power)
{
	sdmmc_cmd_t cmdbuf;

	u32 arg = 0;
	switch (power)
	{
	case SDMMC_POWER_1_8:
		arg = MMC_OCR_HCS | MMC_OCR_VCCQ_18;
		break;

	case SDMMC_POWER_3_3:
		arg = MMC_OCR_HCS | MMC_OCR_VCCQ_27_34;
		break;

	default:
		return 1;
	}

	sdmmc_init_cmd(&cmdbuf, MMC_SEND_OP_COND, arg, SDMMC_RSP_TYPE_3, 0);
	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
		return 1;

	return sdmmc_get_cached_rsp(storage->sdmmc, rocr, SDMMC_RSP_TYPE_3);
}

static int _mmc_storage_get_op_cond(sdmmc_storage_t *storage, u32 power)
{
	u32 timeout = get_tmr_ms() + 1500;

	while (true)
	{
		u32 rocr = 0;
		if (_mmc_storage_get_op_cond_inner(storage, &rocr, power))
			break;

		// Check if power up is done.
		if (rocr & MMC_OCR_BUSY)
		{
			// Check if card is high capacity.
			if (rocr & MMC_OCR_CCS)
				storage->has_sector_access = 1;

			return 0;
		}
		if (get_tmr_ms() > timeout)
			break;

		usleep(1000);
	}

	return 1;
}

static int _mmc_storage_set_relative_addr(sdmmc_storage_t *storage)
{
	return _sdmmc_storage_execute_cmd_ex_state(storage, MMC_SET_RELATIVE_ADDR, storage->rca << 16, 0, R1_SKIP_STATE_CHECK);
}

static void _mmc_storage_parse_cid(sdmmc_storage_t *storage)
{
	u32 *raw_cid = (u32 *)&(storage->raw_cid);

	switch (storage->csd.mmca_vsn)
	{
	case 0: /* MMC v1.0 - v1.2 */
	case 1: /* MMC v1.4 */
		storage->cid.prod_name[6] = unstuff_bits(raw_cid, 48, 8);
		storage->cid.manfid       = unstuff_bits(raw_cid, 104, 24);
		storage->cid.hwrev        = unstuff_bits(raw_cid, 44, 4);
		storage->cid.fwrev        = unstuff_bits(raw_cid, 40, 4);
		storage->cid.serial       = unstuff_bits(raw_cid, 16, 24);
		break;

	case 2: /* MMC v2.0 - v2.2 */
	case 3: /* MMC v3.1 - v3.3 */
	case 4: /* MMC v4 */
		storage->cid.manfid = unstuff_bits(raw_cid, 120, 8);
		storage->cid.oemid  = unstuff_bits(raw_cid, 104, 8);
		storage->cid.prv    = unstuff_bits(raw_cid, 48, 8);
		storage->cid.serial = unstuff_bits(raw_cid, 16, 32);
		break;

	default:
		break;
	}

	storage->cid.prod_name[0] = unstuff_bits(raw_cid, 96, 8);
	storage->cid.prod_name[1] = unstuff_bits(raw_cid, 88, 8);
	storage->cid.prod_name[2] = unstuff_bits(raw_cid, 80, 8);
	storage->cid.prod_name[3] = unstuff_bits(raw_cid, 72, 8);
	storage->cid.prod_name[4] = unstuff_bits(raw_cid, 64, 8);
	storage->cid.prod_name[5] = unstuff_bits(raw_cid, 56, 8);

	storage->cid.month = unstuff_bits(raw_cid, 12, 4);
	storage->cid.year  = unstuff_bits(raw_cid, 8, 4) + 1997;
	if (storage->ext_csd.rev >= 5)
	{
		if (storage->cid.year < 2010)
			storage->cid.year += 16;
	}
}

static void _mmc_storage_parse_csd(sdmmc_storage_t *storage)
{
	u32 *raw_csd = (u32 *)storage->raw_csd;

	storage->csd.mmca_vsn     = unstuff_bits(raw_csd, 122, 4);
	storage->csd.structure    = unstuff_bits(raw_csd, 126, 2);
	storage->csd.cmdclass     = unstuff_bits(raw_csd, 84, 12);
	storage->csd.read_blkbits = unstuff_bits(raw_csd, 80, 4);
	storage->csd.capacity     = (1 + unstuff_bits(raw_csd, 62, 12)) << (unstuff_bits(raw_csd, 47, 3) + 2);
	storage->sec_cnt          = storage->csd.capacity;
}

static void _mmc_storage_parse_ext_csd(sdmmc_storage_t *storage)
{
	u8 *ext_csd = storage->raw_ext_csd;

	storage->ext_csd.rev          = ext_csd[EXT_CSD_REV];
	storage->ext_csd.ext_struct   = ext_csd[EXT_CSD_STRUCTURE];
	storage->ext_csd.card_type    = ext_csd[EXT_CSD_CARD_TYPE];
	storage->ext_csd.dev_version  = *(u16 *)&ext_csd[EXT_CSD_DEVICE_VERSION];
	storage->ext_csd.boot_mult    = ext_csd[EXT_CSD_BOOT_MULT];
	storage->ext_csd.rpmb_mult    = ext_csd[EXT_CSD_RPMB_MULT];
	storage->ext_csd.bkops        = ext_csd[EXT_CSD_BKOPS_SUPPORT];
	storage->ext_csd.bkops_en     = ext_csd[EXT_CSD_BKOPS_EN];

	storage->ext_csd.pre_eol_info   = ext_csd[EXT_CSD_PRE_EOL_INFO];
	storage->ext_csd.dev_life_est_a = ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A];
	storage->ext_csd.dev_life_est_b = ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B];

	storage->ext_csd.cache_size   =  ext_csd[EXT_CSD_CACHE_SIZE]            |
									(ext_csd[EXT_CSD_CACHE_SIZE + 1] << 8)  |
									(ext_csd[EXT_CSD_CACHE_SIZE + 2] << 16) |
									(ext_csd[EXT_CSD_CACHE_SIZE + 3] << 24);

	storage->ext_csd.max_enh_mult = (ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT]             |
									(ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT + 1] << 8)   |
									(ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT + 2] << 16)) *
									 ext_csd[EXT_CSD_HC_WP_GRP_SIZE] * ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];

	storage->sec_cnt = *(u32 *)&ext_csd[EXT_CSD_SEC_CNT];
}

int mmc_storage_get_ext_csd(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_SEND_EXT_CSD, 0, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf = storage->raw_ext_csd;
	reqbuf.blksize = SDMMC_DAT_BLOCKSIZE;
	reqbuf.num_sectors = 1;
	reqbuf.is_write = 0;
	reqbuf.is_multi_block = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
		return 1;

	_mmc_storage_parse_ext_csd(storage);

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static int _mmc_storage_switch(sdmmc_storage_t *storage, u32 arg)
{
	return _sdmmc_storage_execute_cmd_ex_state(storage, MMC_SWITCH, arg, 1, R1_SKIP_STATE_CHECK);
}

static int _mmc_storage_switch_buswidth(sdmmc_storage_t *storage, u32 bus_width)
{
	if (bus_width == SDMMC_BUS_WIDTH_1)
		return 0;

	u32 arg = 0;
	switch (bus_width)
	{
	case SDMMC_BUS_WIDTH_4:
		arg = SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_4);
		break;

	case SDMMC_BUS_WIDTH_8:
		arg = SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_8);
		break;
	}

	if (!_mmc_storage_switch(storage, arg))
		if (!_sdmmc_storage_check_status(storage))
		{
			sdmmc_set_bus_width(storage->sdmmc, bus_width);

			return 0;
		}

	return 1;
}

static int _mmc_storage_enable_HS(sdmmc_storage_t *storage, bool check_sts_before_clk_setup)
{
	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS)))
		return 1;

	if (check_sts_before_clk_setup && _sdmmc_storage_check_status(storage))
		return 1;

	if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_MMC_HS52))
		return 1;

	DPRINTF("[MMC] switched to HS52\n");
	storage->csd.busspeed = 52;

	if (check_sts_before_clk_setup || !_sdmmc_storage_check_status(storage))
		return 0;

	return 1;
}

static int _mmc_storage_enable_HS200(sdmmc_storage_t *storage)
{
	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200)))
		return 1;

	if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_MMC_HS200))
		return 1;

	if (sdmmc_tuning_execute(storage->sdmmc, SDHCI_TIMING_MMC_HS200, MMC_SEND_TUNING_BLOCK_HS200))
		return 1;

	DPRINTF("[MMC] switched to HS200\n");
	storage->csd.busspeed = 200;

	return _sdmmc_storage_check_status(storage);
}

static int _mmc_storage_enable_HS400(sdmmc_storage_t *storage)
{
	if (_mmc_storage_enable_HS200(storage))
		return 1;

	sdmmc_save_tap_value(storage->sdmmc);

	if (_mmc_storage_enable_HS(storage, false))
		return 1;

	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_BUS_WIDTH, EXT_CSD_DDR_BUS_WIDTH_8)))
		return 1;

	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400)))
		return 1;

	if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_MMC_HS400))
		return 1;

	DPRINTF("[MMC] switched to HS400\n");
	storage->csd.busspeed = 400;

	return _sdmmc_storage_check_status(storage);
}

static int _mmc_storage_enable_highspeed(sdmmc_storage_t *storage, u32 card_type, u32 type)
{
	if (sdmmc_get_io_power(storage->sdmmc) != SDMMC_POWER_1_8)
		goto hs52_mode;

	// HS400 needs 8-bit bus width mode.
	if (sdmmc_get_bus_width(storage->sdmmc) == SDMMC_BUS_WIDTH_8 &&
		card_type & EXT_CSD_CARD_TYPE_HS400_1_8V && type == SDHCI_TIMING_MMC_HS400)
		return _mmc_storage_enable_HS400(storage);

	// Try HS200 if HS400 and 4-bit width bus or just HS200.
	if ((sdmmc_get_bus_width(storage->sdmmc) == SDMMC_BUS_WIDTH_8 ||
		 sdmmc_get_bus_width(storage->sdmmc) == SDMMC_BUS_WIDTH_4) &&
		card_type & EXT_CSD_CARD_TYPE_HS200_1_8V &&
		(type == SDHCI_TIMING_MMC_HS400 || type == SDHCI_TIMING_MMC_HS200))
		return _mmc_storage_enable_HS200(storage);

hs52_mode:
	if (card_type & EXT_CSD_CARD_TYPE_HS_52)
		return _mmc_storage_enable_HS(storage, true);

	return 0;
}

/*
static int _mmc_storage_enable_auto_bkops(sdmmc_storage_t *storage)
{
	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_SET_BITS, EXT_CSD_BKOPS_EN, EXT_CSD_BKOPS_AUTO)))
		return 1;

	return _sdmmc_storage_check_status(storage);
}
*/

int sdmmc_storage_init_mmc(sdmmc_storage_t *storage, sdmmc_t *sdmmc, u32 bus_width, u32 type)
{
	memset(storage, 0, sizeof(sdmmc_storage_t));
	storage->sdmmc = sdmmc;
	storage->rca = 2; // Set default device address. This could be a config item.

	DPRINTF("[MMC]-[init: bus: %d, type: %d]\n", bus_width, type);

	if (sdmmc_init(sdmmc, SDMMC_4, SDMMC_POWER_1_8, SDMMC_BUS_WIDTH_1, SDHCI_TIMING_MMC_ID))
		return 1;
	DPRINTF("[MMC] after init\n");

	// Wait 1ms + 74 cycles.
	usleep(1000 + (74 * 1000 + sdmmc->card_clock - 1) / sdmmc->card_clock);

	if (_sdmmc_storage_go_idle_state(storage))
		return 1;
	DPRINTF("[MMC] went to idle state\n");

	if (_mmc_storage_get_op_cond(storage, SDMMC_POWER_1_8))
		return 1;
	DPRINTF("[MMC] got op cond\n");

	if (_sdmmc_storage_get_cid(storage))
		return 1;
	DPRINTF("[MMC] got cid\n");

	if (_mmc_storage_set_relative_addr(storage))
		return 1;
	DPRINTF("[MMC] set relative addr\n");

	if (_sdmmc_storage_get_csd(storage))
		return 1;
	DPRINTF("[MMC] got csd\n");
	_mmc_storage_parse_csd(storage);

	if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_MMC_LS26))
		return 1;
	DPRINTF("[MMC] after setup clock\n");

	if (_sdmmc_storage_select_card(storage))
		return 1;
	DPRINTF("[MMC] card selected\n");

	if (_sdmmc_storage_set_blocklen(storage, EMMC_BLOCKSIZE))
		return 1;
	DPRINTF("[MMC] set blocklen to EMMC_BLOCKSIZE\n");

	// Check system specification version, only version 4.0 and later support below features.
	if (storage->csd.mmca_vsn < CSD_SPEC_VER_4)
		goto done;

	if (_mmc_storage_switch_buswidth(storage, bus_width))
		return 1;
	DPRINTF("[MMC] switched buswidth\n");

	if (mmc_storage_get_ext_csd(storage))
		return 1;
	DPRINTF("[MMC] got ext_csd\n");

	_mmc_storage_parse_cid(storage); // This needs to be after csd and ext_csd.

/*
	if (storage->cid.manfid == 0x11 && storage->ext_csd.bkops && !(storage->ext_csd.bkops_en & EXT_CSD_BKOPS_AUTO))
	{
		_mmc_storage_enable_auto_bkops(storage);
		DPRINTF("[MMC] BKOPS enabled\n");
	}
*/

	if (_mmc_storage_enable_highspeed(storage, storage->ext_csd.card_type, type))
		return 1;
	DPRINTF("[MMC] successfully switched to HS mode\n");

	sdmmc_card_clock_powersave(storage->sdmmc, SDMMC_POWER_SAVE_ENABLE);

done:
	storage->initialized = 1;

	return 0;
}

int sdmmc_storage_set_mmc_partition(sdmmc_storage_t *storage, u32 partition)
{
	if (_mmc_storage_switch(storage, SDMMC_SWITCH(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_PART_CONFIG, partition)))
		return 1;

	if (_sdmmc_storage_check_status(storage))
		return 1;

	storage->partition = partition;

	return 0;
}

/*
 * SD specific functions.
 */

static int _sd_storage_execute_app_cmd(sdmmc_storage_t *storage, u32 expected_state, u32 mask, sdmmc_cmd_t *cmdbuf, sdmmc_req_t *req, u32 *blkcnt_out)
{
	u32 tmp;
	if (_sdmmc_storage_execute_cmd_ex_masked(storage, &tmp, MMC_APP_CMD, storage->rca << 16, 0, expected_state, mask))
		return 1;

	return sdmmc_execute_cmd(storage->sdmmc, cmdbuf, req, blkcnt_out);
}

static int _sd_storage_execute_app_cmd_tran(sdmmc_storage_t *storage, u32 cmd, u32 arg)
{
	if (_sdmmc_storage_execute_cmd_ex_state(storage, MMC_APP_CMD, storage->rca << 16, 0, R1_STATE_TRAN))
		return 1;

	return _sdmmc_storage_execute_cmd_ex_state(storage, cmd, arg, 0, R1_STATE_TRAN);
}

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
void _sd_storage_debug_print_cid(const u32 *raw_cid)
{
	gfx_printf("Card Identification\n");

	gfx_printf("MID:                   %02X\n", unstuff_bits(raw_cid, 120, 8));
	gfx_printf("OID                    %04X\n", unstuff_bits(raw_cid, 104, 16));
	gfx_printf("PNM:                   %02X %02X %02X %02X %02X\n",
		unstuff_bits(raw_cid, 96, 8), unstuff_bits(raw_cid, 88, 8),
		unstuff_bits(raw_cid, 80, 8), unstuff_bits(raw_cid, 72, 8),
		unstuff_bits(raw_cid, 64, 8));
	gfx_printf("PRV:                   %02X\n", unstuff_bits(raw_cid, 56, 8));
	gfx_printf("PSN:                   %08X\n", unstuff_bits(raw_cid, 24, 32));
	gfx_printf("MDT:                   %03X\n", unstuff_bits(raw_cid, 8, 12));
	gfx_printf("--RSVD--               %X\n",   unstuff_bits(raw_cid, 20, 4));
}

void _sd_storage_debug_print_csd(const u32 *raw_csd)
{
	gfx_printf("\n");

	gfx_printf("\nCSD_STRUCTURE:         %X\n", unstuff_bits(raw_csd, 126, 2));
	gfx_printf("TAAC:                  %02X\n", unstuff_bits(raw_csd, 112, 8));
	gfx_printf("NSAC:                  %02X\n", unstuff_bits(raw_csd, 104, 8));
	gfx_printf("TRAN_SPEED:            %02X\n", unstuff_bits(raw_csd, 96, 8));
	gfx_printf("CCC:                   %03X\n", unstuff_bits(raw_csd, 84, 12));
	gfx_printf("READ_BL_LEN:           %X\n",   unstuff_bits(raw_csd, 80, 4));
	gfx_printf("READ_BL_PARTIAL:       %X\n",   unstuff_bits(raw_csd, 79, 1));
	gfx_printf("WRITE_BLK_MISALIGN:    %X\n",   unstuff_bits(raw_csd, 78, 1));
	gfx_printf("READ_BLK_MISALIGN:     %X\n",   unstuff_bits(raw_csd, 77, 1));
	gfx_printf("DSR_IMP:               %X\n",   unstuff_bits(raw_csd, 76, 1));
	gfx_printf("C_SIZE:                %06X\n", unstuff_bits(raw_csd, 48, 28)); // CSD 3 (SDUC).

	gfx_printf("ERASE_BLK_LEN:         %X\n",   unstuff_bits(raw_csd, 46, 1));
	gfx_printf("SECTOR_SIZE:           %02X\n", unstuff_bits(raw_csd, 39, 6));
	gfx_printf("WP_GRP_SIZE:           %02X\n", unstuff_bits(raw_csd, 32, 6));
	gfx_printf("WP_GRP_ENABLE:         %X\n",   unstuff_bits(raw_csd, 31, 1));

	gfx_printf("R2W_FACTOR:            %X\n",   unstuff_bits(raw_csd, 26, 3));
	gfx_printf("WRITE_BL_LEN:          %X\n",   unstuff_bits(raw_csd, 22, 4));
	gfx_printf("WRITE_BL_PARTIAL:      %X\n",   unstuff_bits(raw_csd, 21, 1));

	gfx_printf("FILE_FORMAT_GRP:       %X\n",   unstuff_bits(raw_csd, 15, 1));
	gfx_printf("COPY:                  %X\n",   unstuff_bits(raw_csd, 14, 1));
	gfx_printf("PERM_WRITE_PROTECT:    %X\n",   unstuff_bits(raw_csd, 13, 1));
	gfx_printf("TMP_WRITE_PROTECT:     %X\n",   unstuff_bits(raw_csd, 12, 1));
	gfx_printf("FILE_FORMAT:           %X\n",   unstuff_bits(raw_csd, 10, 2));

	gfx_printf("--RSVD--               %02X %X %X %02X %X\n",
		unstuff_bits(raw_csd, 120, 6),
		unstuff_bits(raw_csd, 47, 1),  unstuff_bits(raw_csd, 29, 2),
		unstuff_bits(raw_csd, 16, 5),  unstuff_bits(raw_csd, 8, 2));
}

void _sd_storage_debug_print_scr(const u32 *raw_scr)
{
	u32 scr[4];
	memcpy(&scr[2], raw_scr, 8);

	gfx_printf("\n");

	gfx_printf("SCR_STRUCTURE:         %X\n",   unstuff_bits(scr, 60, 4));
	gfx_printf("SD_SPEC:               %X\n",   unstuff_bits(scr, 56, 4));
	gfx_printf("DATA_STAT_AFTER_ERASE: %X\n",   unstuff_bits(scr, 55, 1));
	gfx_printf("SD_SECURITY:           %X\n",   unstuff_bits(scr, 52, 3));
	gfx_printf("SD_BUS widths:         %X\n",   unstuff_bits(scr, 48, 4));
	gfx_printf("SD_SPEC3:              %X\n",   unstuff_bits(scr, 47, 1));
	gfx_printf("EX_SECURITY:           %X\n",   unstuff_bits(scr, 43, 4));
	gfx_printf("SD_SPEC4:              %X\n",   unstuff_bits(scr, 42, 1));
	gfx_printf("SD_SPECX:              %X\n",   unstuff_bits(scr, 38, 4));
	gfx_printf("CMD_SUPPORT:           %X\n",   unstuff_bits(scr, 32, 4));
	gfx_printf("VENDOR:                %08X\n", unstuff_bits(scr, 0, 32));
	gfx_printf("--RSVD--               %X\n",   unstuff_bits(scr, 36, 2));
}

void _sd_storage_debug_print_ssr(const u8 *raw_ssr)
{
	u32 raw_ssr0[4]; // 511:384.
	u32 raw_ssr1[4]; // 383:256.
	u32 raw_ssr2[4]; // 255:128.
	u32 raw_ssr3[4]; // 127:0.
	memcpy(raw_ssr0, &raw_ssr[0],  16);
	memcpy(raw_ssr1, &raw_ssr[16], 16);
	memcpy(raw_ssr2, &raw_ssr[32], 16);
	memcpy(raw_ssr3, &raw_ssr[48], 16);

	gfx_printf("\nSD Status:\n");

	gfx_printf("DAT_BUS_WIDTH:         %X\n",   unstuff_bits(raw_ssr0, 510, 2));
	gfx_printf("SECURED_MODE:          %X\n",   unstuff_bits(raw_ssr0, 509, 1));
	gfx_printf("SECURITY_FUNCTIONS:    %02X\n", unstuff_bits(raw_ssr0, 502, 6));
	gfx_printf("SD_CARD_TYPE:          %04X\n", unstuff_bits(raw_ssr0, 480, 16));
	gfx_printf("SZ_OF_PROTECTED_AREA:  %08X\n", unstuff_bits(raw_ssr0, 448, 32));
	gfx_printf("SPEED_CLASS:           %02X\n", unstuff_bits(raw_ssr0, 440, 8));
	gfx_printf("PERFORMANCE_MOVE:      %02X\n", unstuff_bits(raw_ssr0, 432, 8));
	gfx_printf("AU_SIZE:               %X\n",   unstuff_bits(raw_ssr0, 428, 4));
	gfx_printf("ERAZE_SIZE:            %04X\n", unstuff_bits(raw_ssr0, 408, 16));
	gfx_printf("ERASE_TIMEOUT:         %02X\n", unstuff_bits(raw_ssr0, 402, 6));
	gfx_printf("ERASE_OFFSET:          %X\n",   unstuff_bits(raw_ssr0, 400, 2));
	gfx_printf("UHS_SPEED_GRADE:       %X\n",   unstuff_bits(raw_ssr0, 396, 4));
	gfx_printf("UHS_AU_SIZE:           %X\n",   unstuff_bits(raw_ssr0, 392, 4));
	gfx_printf("VIDEO_SPEED_CLASS:     %02X\n", unstuff_bits(raw_ssr0, 384, 8));

	gfx_printf("VSC_AU_SIZE:           %03X\n", unstuff_bits(raw_ssr1, 368, 10));
	gfx_printf("SUS_ADDR:              %06X\n", unstuff_bits(raw_ssr1, 346, 22));
	gfx_printf("APP_PERF_CLASS:        %X\n",   unstuff_bits(raw_ssr1, 336, 4));
	gfx_printf("PERFORMANCE_ENHANCE:   %02X\n", unstuff_bits(raw_ssr1, 328, 8));
	gfx_printf("DISCARD_SUPPORT:       %X\n",   unstuff_bits(raw_ssr1, 313, 1));
	gfx_printf("FULE_SUPPORT:          %X\n",   unstuff_bits(raw_ssr1, 312, 1));

	gfx_printf("VENDOR_1: %06X   %08X\n",
		unstuff_bits(raw_ssr1, 288, 24),  unstuff_bits(raw_ssr1, 256, 32));

	gfx_printf("VENDOR_2: %08X %08X %08X %08X\n",
		unstuff_bits(raw_ssr2, 224, 32),  unstuff_bits(raw_ssr2, 192, 32),
		unstuff_bits(raw_ssr2, 160, 32),  unstuff_bits(raw_ssr2, 128, 32));
	gfx_printf("VENDOR_3: %08X %08X %08X %08X\n",
		unstuff_bits(raw_ssr3, 96 - 0, 32),     unstuff_bits(raw_ssr3, 64, 32),
		unstuff_bits(raw_ssr3, 32 - 0, 32),     unstuff_bits(raw_ssr3, 0, 32));

	gfx_printf("--RSVD--               %02X %X %02X %02X %04X\n",
		unstuff_bits(raw_ssr0, 496, 6),   unstuff_bits(raw_ssr0, 424, 4),
		unstuff_bits(raw_ssr1, 378, 6),   unstuff_bits(raw_ssr1, 340, 6),
		unstuff_bits(raw_ssr1, 314, 14));
}
#endif

void sd_storage_get_vendor_info(sdmmc_storage_t *storage, sd_vendor_info_t *info)
{
	// CID Reserved.
	info->cid_rsvd = storage->cid.rsvd;

	// CSD Reserved.
	info->csd_rsvd8_9     = unstuff_bits((u32 *)storage->raw_csd,   8, 2);
	info->csd_rsvd16_20   = unstuff_bits((u32 *)storage->raw_csd,  16, 5);
	info->csd_rsvd29_30   = unstuff_bits((u32 *)storage->raw_csd,  29, 2);
	info->csd_rsvd120_125 = unstuff_bits((u32 *)storage->raw_csd, 120, 6);

	// SCR Vendor and Reserved.
	u32 scr[4];
	memcpy(&scr[2], storage->raw_scr, 8);
	info->scr_vendor = storage->scr.vendor;
	info->scr_rsvd   = unstuff_bits(scr, 36, 2);

	// SSR Vendor and Reserved.
	u32 raw_ssr0[4]; // 511:384.
	u32 raw_ssr1[4]; // 383:256.
	u32 raw_ssr2[4]; // 255:128.
	u32 raw_ssr3[4]; // 127:0.
	memcpy(raw_ssr0, &storage->raw_ssr[0],  16);
	memcpy(raw_ssr1, &storage->raw_ssr[16], 16);
	memcpy(raw_ssr2, &storage->raw_ssr[32], 16);
	memcpy(raw_ssr3, &storage->raw_ssr[48], 16);
	info->ssr_vendor0_31    = unstuff_bits(raw_ssr3,   0, 32);
	info->ssr_vendor32_63   = unstuff_bits(raw_ssr3,  32, 32);
	info->ssr_vendor64_95   = unstuff_bits(raw_ssr3,  64, 32);
	info->ssr_vendor96_127  = unstuff_bits(raw_ssr3,  96, 32);
	info->ssr_vendor128_159 = unstuff_bits(raw_ssr2, 128, 32);
	info->ssr_vendor160_191 = unstuff_bits(raw_ssr2, 160, 32);
	info->ssr_vendor192_223 = unstuff_bits(raw_ssr2, 192, 32);
	info->ssr_vendor224_255 = unstuff_bits(raw_ssr2, 224, 32);
	info->ssr_vendor256_287 = unstuff_bits(raw_ssr1, 256, 32);
	info->ssr_vendor288_311 = unstuff_bits(raw_ssr1, 288, 24);

	info->ssr_rsvd314_327 = unstuff_bits(raw_ssr1, 314, 14);
	info->ssr_rsvd340_345 = unstuff_bits(raw_ssr1, 340,  6);
	info->ssr_rsvd378_383 = unstuff_bits(raw_ssr1, 378,  6);
	info->ssr_rsvd424_427 = unstuff_bits(raw_ssr0, 424,  4);
	info->ssr_rsvd496_501 = unstuff_bits(raw_ssr0, 496,  6);
}

int sd_storage_get_ext_reg(sdmmc_storage_t *storage, u8 fno, u8 page, u16 address, u32 len, void *buf)
{
	if (!(storage->scr.cmds & BIT(2)))
		return 1;

	sdmmc_cmd_t cmdbuf;

	u32 arg = fno << 27 | page << 18 | address << 9 | (len - 1);

	sdmmc_init_cmd(&cmdbuf, SD_READ_EXTR_SINGLE, arg, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf = buf;
	reqbuf.blksize = SDMMC_DAT_BLOCKSIZE;
	reqbuf.num_sectors = 1;
	reqbuf.is_write = 0;
	reqbuf.is_multi_block = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
		return 1;

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static int _sd_storage_send_if_cond(sdmmc_storage_t *storage, bool *is_sd_v1)
{
	sdmmc_cmd_t cmdbuf;
	u16 icr = SD_ICR_PCIE | SD_ICR_VHS_27_36 | SD_ICR_PATTERN;
	sdmmc_init_cmd(&cmdbuf, SD_SEND_IF_COND, icr, SDMMC_RSP_TYPE_7, 0);

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
	{
		// The SD Card is version 1.XX if there is no response.
		if (storage->sdmmc->error_sts == SDHCI_ERR_INT_CMD_TIMEOUT)
		{
			*is_sd_v1 = true;

			return 0;
		}

		return 1;
	}

	// For version >= 2.0, parse results.
	u32 ricr = 0;
	sdmmc_get_cached_rsp(storage->sdmmc, &ricr, SDMMC_RSP_TYPE_7);

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	gfx_printf("ICR:                   %08X -> %08X\n", icr, ricr);
#endif

	// Check if VHS was accepted and pattern was properly returned.
	if ((ricr & 0xFFF) == (icr & 0xFFF))
	{
		storage->has_pcie = ricr & SD_ICR_PCIE;

		return 0;
	}

	return 1;
}

static int _sd_storage_send_op_cond(sdmmc_storage_t *storage, u32 *rocr, u32 ocr, bool is_sd_v1)
{
	sdmmc_cmd_t cmdbuf;

	sdmmc_init_cmd(&cmdbuf, SD_APP_SD_SEND_OP_COND, ocr, SDMMC_RSP_TYPE_3, 0);

	if (_sd_storage_execute_app_cmd(storage, R1_SKIP_STATE_CHECK, is_sd_v1 ? R1_ILLEGAL_COMMAND : 0, &cmdbuf, NULL, NULL))
		return 1;

	return sdmmc_get_cached_rsp(storage->sdmmc, rocr, SDMMC_RSP_TYPE_3);
}

static int _sd_storage_get_op_cond(sdmmc_storage_t *storage, bool is_sd_v1, int lv_support)
{
	u32 timeout = get_tmr_ms() + 1500;

	// Host in 3.3V power supply (VDD1).
	u32 ocr = SD_OCR_VDD_32_33;

	if (!is_sd_v1)
	{
		// Support for Current > 150mA.
		ocr |= SD_OCR_XPC;
		// Support for handling block-addressed SDHC/SDXC/SDUC cards.
		ocr	|= SD_OCR_HCS;
		// Support and request 1.8V signaling.
		ocr |= lv_support ? SD_OCR_S18R : 0;
	}

	while (true)
	{
		u32 rocr = 0;
		if (_sd_storage_send_op_cond(storage, &rocr, ocr, is_sd_v1))
			break;

		// Check if power up is done.
		if (rocr & SD_OCR_BUSY)
		{
#ifdef SDMMC_DEBUG_PRINT_SD_REGS
			gfx_printf("ROCR:                  %08X (%d)\n", rocr, lv_support);
#else
			DPRINTF("[SD] rocr: %08X, lv: %d\n", rocr, lv_support);
#endif

			// Check if card is higher capacity.
			if (rocr & SD_OCR_CCS)
				storage->has_sector_access = 1; // Non-SDUC.

			// Check if card accepted 1.8V signaling.
			if (rocr & SD_OCR_S18A && lv_support)
			{
				// Switch to 1.8V signaling.
				if (!_sdmmc_storage_execute_cmd_ex_state(storage, SD_VOLTAGE_SWITCH, 0, 0, R1_STATE_READY))
				{
					if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_UHS_SDR12))
						return 1;

					if (sdmmc_enable_low_voltage(storage->sdmmc))
						return 1;

					storage->is_low_voltage = 1;

					DPRINTF("-> switched to low voltage\n");
				}
			}
			else
			{
				DPRINTF("[SD] no low voltage support\n");
			}

			return 0;
		}
		if (get_tmr_ms() > timeout)
			break;
		msleep(10); // Needs to be at least 10ms for some SD Cards
	}

	return 1;
}

static int _sd_storage_get_rca(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, SD_SEND_RELATIVE_ADDR, 0, SDMMC_RSP_TYPE_6, 0);

	u32 timeout = get_tmr_ms() + 1500;

	while (true)
	{
		if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, NULL, NULL))
			break;

		u32 rca = 0;
		if (sdmmc_get_cached_rsp(storage->sdmmc, &rca, SDMMC_RSP_TYPE_6))
			break;

		if (rca >> 16)
		{
			storage->rca = rca >> 16;
			return 0;
		}

		if (get_tmr_ms() > timeout)
			break;
		usleep(1000);
	}

	return 1;
}

u8 sd_storage_get_scr_sda_ver(sdmmc_storage_t *storage)
{
	u8 version = 0;

	u8 sda_spec  = storage->scr.sda_vsn;
	u8 sda_spec3 = storage->scr.sda_spec & 1;
	u8 sda_spec4 = (storage->scr.sda_spec >> 1) & 1;
	u8 sda_specx = storage->scr.sda_spec >> 4;

	switch (sda_spec)
	{
	case 0:
		version = 1;
		break;

	case 1:
		version = 1;
		break;

	case 2:
		if (!sda_spec3)
			version = 2;
		else if (!sda_spec4 && !sda_specx)
			version = 3;
		else
			version = sda_specx + 4;
		break;
	}

	return version;
}

static void _sd_storage_parse_scr(sdmmc_storage_t *storage)
{
	// unstuff_bits can parse only 4 u32
	u32 scr[4];
	u8 sda_spec3 = 0;
	u8 sda_spec4 = 0;
	u8 sda_specx = 0;

	memcpy(&scr[2], storage->raw_scr, 8);

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	_sd_storage_debug_print_scr((u32 *)storage->raw_scr);
#endif

	storage->scr.sda_vsn = unstuff_bits(scr, 56, 4);
	storage->scr.bus_widths = unstuff_bits(scr, 48, 4);

	// If v2.0 is supported, check if Physical Layer Spec v3.0 is supported.
	if (storage->scr.sda_vsn == SCR_SPEC_VER_2)
		sda_spec3 = unstuff_bits(scr, 47, 1);
	if (sda_spec3)
	{
		sda_spec4 = unstuff_bits(scr, 42, 1);
		sda_specx = unstuff_bits(scr, 38, 4);
		if (sda_spec4)
			storage->scr.cmds = unstuff_bits(scr, 32, 4);
		else
			storage->scr.cmds = unstuff_bits(scr, 32, 2);
	}

	storage->scr.sda_spec = sda_spec3 | (sda_spec4 << 1) | (sda_specx << 4);

	storage->scr.vendor = unstuff_bits(scr, 0, 32);
}

int sd_storage_get_scr(sdmmc_storage_t *storage)
{
	u8 buf[8] __attribute__ ((aligned(SDMMC_ADMA_ADDR_ALIGN)));

	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, SD_APP_SEND_SCR, 0, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf              = buf;
	reqbuf.blksize          = 8;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = 0;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (_sd_storage_execute_app_cmd(storage, R1_STATE_TRAN, 0, &cmdbuf, &reqbuf, NULL))
		return 1;

	//Prepare buffer for unstuff_bits
	for (u32 i = 0; i < 8; i += 4)
	{
		storage->raw_scr[i + 3] = buf[i];
		storage->raw_scr[i + 2] = buf[i + 1];
		storage->raw_scr[i + 1] = buf[i + 2];
		storage->raw_scr[i]     = buf[i + 3];
	}
	_sd_storage_parse_scr(storage);

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static int _sd_storage_switch_get_all(sdmmc_storage_t *storage, void *buf)
{
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, SD_SWITCH, 0xFFFFFF, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf              = buf;
	reqbuf.blksize          = SDMMC_CMD_BLOCKSIZE;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = 0;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
		return 1;

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static int _sd_storage_switch(sdmmc_storage_t *storage, void *buf, int mode, int group, u32 arg)
{
	sdmmc_cmd_t cmdbuf;
	u32 switchcmd = mode << 31 | 0x00FFFFFF;
	switchcmd &= ~(0xF << (group * 4));
	switchcmd |= arg << (group * 4);
	sdmmc_init_cmd(&cmdbuf, SD_SWITCH, switchcmd, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf              = buf;
	reqbuf.blksize          = SDMMC_CMD_BLOCKSIZE;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = 0;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
		return 1;

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static void _sd_storage_set_power_limit(sdmmc_storage_t *storage, u16 power_limit, u8 *buf)
{
	u32 pwr = SD_SET_POWER_LIMIT_0_72;

	// If UHS-I only, anything above 1.44W defaults to 1.44W.
	/*
	if (power_limit & SD_MAX_POWER_2_88)
		pwr = SD_SET_POWER_LIMIT_2_88;
	else if (power_limit & SD_MAX_POWER_2_16)
		pwr = SD_SET_POWER_LIMIT_2_16;
	*/
	if (power_limit & SD_MAX_POWER_1_44)
		pwr = SD_SET_POWER_LIMIT_1_44;

	_sd_storage_switch(storage, buf, SD_SWITCH_SET, SD_SWITCH_GRP_PWRLIM, pwr);

	switch ((buf[15] >> 4) & 0x0F)
	{
	/*
	case SD_SET_POWER_LIMIT_2_88:
		DPRINTF("[SD] power limit raised to 2880 mW\n");
		break;

	case SD_SET_POWER_LIMIT_2_16:
		DPRINTF("[SD] power limit raised to 2160 mW\n");
		break;
	*/
	case SD_SET_POWER_LIMIT_1_44:
		DPRINTF("[SD] power limit raised to 1440 mW\n");
		break;

	default:
	case SD_SET_POWER_LIMIT_0_72:
		DPRINTF("[SD] power limit defaulted to 720 mW\n");
		break;
	}
}

__attribute__ ((unused)) static int _sd_storage_set_driver_type(sdmmc_storage_t *storage, u32 driver, u8 *buf)
{
	if (_sd_storage_switch(storage, buf, SD_SWITCH_CHECK, SD_SWITCH_GRP_DRVSTR, driver))
		return 1;

	u32 driver_out = buf[15] & 0xF;
	if (driver_out != driver)
		return 1;
	DPRINTF("[SD] supports Driver Strength %d\n", driver);

	if (_sd_storage_switch(storage, buf, SD_SWITCH_SET, SD_SWITCH_GRP_DRVSTR, driver))
		return 1;

	if (driver_out != (buf[15] & 0xF))
		return 1;
	DPRINTF("[SD] card accepted Driver Strength %d\n", driver);

	sdmmc_setup_drv_type(storage->sdmmc, driver);

	return 0;
}

/*
 * SD Card DDR200 (DDR208) support
 *
 * DLL Tuning (a) or Tuning Window (b) procedure:
 * 1. Check that Vendor Specific Command System is supported.
 *    Used as Enable DDR200 Bus.
 * 2. Enable DDR200 bus mode via setting 14 to Group 2 via CMD6.
 *    Access Mode group is left to default 0 (SDR12).
 * 3. Setup clock to 200 or 208 MHz.
 * 4a. Set host to DDR200/HS400 bus mode that enables DLL syncing.
 *     Actual implementation supported by all DDR200 cards.
 * --
 * 4b. Set host to DDR50 bus mode that supports such high clocks.
 *     Execute Manual Tuning.
 *     Limited to non-Sandisk cards.
 *
 * On Tegra SoCs, that can be done with DDR50 host mode.
 * That's because HS400 4-bit or HS400 generally, is not supported on SD SDMMC.
 * And also, tuning can't be done automatically on any DDR mode.
 * So it needs to be done manually and selected tap will be applied from the
 * biggest sampling window.
 * That allows DDR200 support on every DDR200 SD card, other than the original
 * maker of DDR200, Sandisk.
 *
 * On the original implementation of DDR200 from Sandisk, a DLL mechanism,
 * like the one in eMMC HS400 is mandatory.
 * So the card can start data signals whenever it wants, and the host should
 * synchronize to the first DAT signal edge change.
 * Every single other vendor that implemented that, always starts data transfers
 * aligned to clock. That basically makes DDR200 in such SD cards a SDR104 but
 * sampled on both edges. So effectively, it's an in-spec signal with DDR50,
 * only that is clocked at 200MHz, instead of 50MHz.
 * So the extra needed thing is using a tuning window, which is absent from the
 * original implementation, since DDL syncing does not use that.
 *
 * On DLL tuning method expected cards, the tuning window is tiny.
 * So check against a minimum of 8 taps window, to disallow DDR200.
 */
#ifdef BDK_SDMMC_UHS_DDR200_SUPPORT
static int _sd_storage_enable_DDR200(sdmmc_storage_t *storage, u8 *buf)
{
	u32 cmd_system = UHS_DDR200_BUS_SPEED;
	if (_sd_storage_switch(storage, buf, SD_SWITCH_CHECK, SD_SWITCH_GRP_CMDSYS, cmd_system))
		return 1;

	u32 system_out = (buf[16] >> 4) & 0xF;
	if (system_out != cmd_system)
		return 1;
	DPRINTF("[SD] supports DDR200 mode\n");

	u16 total_pwr_consumption = ((u16)buf[0] << 8) | buf[1];
	DPRINTF("[SD] max power: %d mW\n", total_pwr_consumption * 3600 / 1000);
	storage->max_power = total_pwr_consumption;

	// Check if total is low than max and switch.
	if (total_pwr_consumption <= 800)
	{
		if (_sd_storage_switch(storage, buf, SD_SWITCH_SET, SD_SWITCH_GRP_CMDSYS, cmd_system))
			return 1;

		if (system_out != ((buf[16] >> 4) & 0xF))
			return 1;
		DPRINTF("[SD] card accepted DDR200\n");

		if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_UHS_DDR200))
			return 1;
		DPRINTF("[SD] after setup clock DDR200\n");

		if (sdmmc_tuning_execute(storage->sdmmc, SDHCI_TIMING_UHS_DDR200, SD_SEND_TUNING_BLOCK))
			return 1;
		DPRINTF("[SD] after tuning DDR200\n");

		return _sdmmc_storage_check_status(storage);
	}

	DPRINTF("[SD] card max power over limit\n");
	return 1;
}
#endif

static int _sd_storage_set_card_bus_speed(sdmmc_storage_t *storage, u32 hs_type, u8 *buf)
{
	if (_sd_storage_switch(storage, buf, SD_SWITCH_CHECK, SD_SWITCH_GRP_ACCESS, hs_type))
		return 1;

	u32 type_out = buf[16] & 0xF;
	if (type_out != hs_type)
		return 1;
	DPRINTF("[SD] supports selected (U)HS mode %d\n", buf[16] & 0xF);

	u16 total_pwr_consumption = ((u16)buf[0] << 8) | buf[1];
	DPRINTF("[SD] max power: %d mW\n", total_pwr_consumption * 3600 / 1000);
	storage->max_power = total_pwr_consumption;

	// Check if total is low than max and switch.
	if (total_pwr_consumption <= 800)
	{
		if (_sd_storage_switch(storage, buf, SD_SWITCH_SET, SD_SWITCH_GRP_ACCESS, hs_type))
			return 1;

		if (type_out != (buf[16] & 0xF))
			return 1;

		return 0;
	}

	DPRINTF("[SD] card max power over limit\n");
	return 1;
}

int sd_storage_get_fmodes(sdmmc_storage_t *storage, u8 *buf, sd_func_modes_t *fmodes)
{
	if (!buf)
		buf = (u8 *)SDMMC_ALT_DMA_BUFFER;

	if (_sd_storage_switch_get_all(storage, buf))
		return 1;

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	gfx_printf("\nFMD:       %04X %04X %04X %04X %04X %04X\n",
		buf[13] | (buf[12] << 8),
		buf[11] | (buf[10] << 8),
		buf[9]  | (buf[8]  << 8),
		buf[7]  | (buf[6]  << 8),
		buf[5]  | (buf[4]  << 8),
		buf[3]  | (buf[2]  << 8));
#endif

	fmodes->access_mode     = buf[13] | (buf[12] << 8);
	fmodes->cmd_system      = buf[11] | (buf[10] << 8);
	fmodes->driver_strength = buf[9]  | (buf[8]  << 8);
	fmodes->power_limit     = buf[7]  | (buf[6]  << 8);

	return 0;
}

static int _sd_storage_set_uhs_bus_speed(sdmmc_storage_t *storage, u32 type)
{
	sd_func_modes_t fmodes;
	u8 *buf = storage->raw_ext_csd;

	if (sdmmc_get_bus_width(storage->sdmmc) != SDMMC_BUS_WIDTH_4)
		return 1;

	if (sd_storage_get_fmodes(storage, buf, &fmodes))
		return 1;

#ifdef BDK_SDMMC_UHS_DDR200_SUPPORT
	DPRINTF("[SD] access: %02X, power: %02X, cmd: %02X\n", fmodes.access_mode, fmodes.power_limit, fmodes.cmd_system);
#else
	DPRINTF("[SD] access: %02X, power: %02X\n", fmodes.access_mode, fmodes.power_limit);
#endif

	u32 hs_type = 0;
	switch (type)
	{
#ifdef BDK_SDMMC_UHS_DDR200_SUPPORT
	case SDHCI_TIMING_UHS_DDR200:
		// Fall through if DDR200 is not supported.
		if (fmodes.cmd_system & SD_MODE_UHS_DDR200)
		{
			DPRINTF("[SD] setting bus speed to DDR200\n");
			storage->csd.busspeed = 200;
			_sd_storage_set_power_limit(storage, fmodes.power_limit, buf);
			return _sd_storage_enable_DDR200(storage, buf);
		}
#endif

	case SDHCI_TIMING_UHS_SDR104:
	case SDHCI_TIMING_UHS_SDR82:
		// Fall through if not supported.
		if (fmodes.access_mode & SD_MODE_UHS_SDR104)
		{
			type    = SDHCI_TIMING_UHS_SDR104;
			hs_type = UHS_SDR104_BUS_SPEED;
			DPRINTF("[SD] setting bus speed to SDR104\n");
			switch (type)
			{
			case SDHCI_TIMING_UHS_SDR104:
				storage->csd.busspeed = 104;
				break;
			case SDHCI_TIMING_UHS_SDR82:
				storage->csd.busspeed = 82;
				break;
			}
			break;
		}

	case SDHCI_TIMING_UHS_SDR50:
		if (fmodes.access_mode & SD_MODE_UHS_SDR50)
		{
			type    = SDHCI_TIMING_UHS_SDR50;
			hs_type = UHS_SDR50_BUS_SPEED;
			DPRINTF("[SD] setting bus speed to SDR50\n");
			storage->csd.busspeed = 50;
			break;
		}
/*
	case SDHCI_TIMING_UHS_DDR50:
		if (fmodes.access_mode & SD_MODE_UHS_DDR50)
		{
			type    = SDHCI_TIMING_UHS_DDR50;
			hs_type = UHS_DDR50_BUS_SPEED;
			DPRINTF("[SD] setting bus speed to DDR50\n");
			storage->csd.busspeed = 50;
			break;
		}
*/
	case SDHCI_TIMING_UHS_SDR25:
		if (fmodes.access_mode & SD_MODE_UHS_SDR25)
		{
			type = SDHCI_TIMING_UHS_SDR25;
			hs_type = UHS_SDR25_BUS_SPEED;
			DPRINTF("[SD] setting bus speed to SDR25\n");
			storage->csd.busspeed = 25;
			break;
		}

	default:
		DPRINTF("[SD] bus speed defaulted to SDR12\n");
		storage->csd.busspeed = 12;
		return 0; // Already set.
	}

	// Try to raise the power limit to let the card perform better.
	if (hs_type != UHS_SDR25_BUS_SPEED) // Not applicable for SDR12/SDR25.
		_sd_storage_set_power_limit(storage, fmodes.power_limit, buf);

	// Setup and set selected card and bus speed.
	if (_sd_storage_set_card_bus_speed(storage, hs_type, buf))
		return 1;
	DPRINTF("[SD] card accepted UHS\n");

	if (sdmmc_setup_clock(storage->sdmmc, type))
		return 1;
	DPRINTF("[SD] after setup clock\n");

	if (sdmmc_tuning_execute(storage->sdmmc, type, SD_SEND_TUNING_BLOCK))
		return 1;
	DPRINTF("[SD] after tuning\n");

	return _sdmmc_storage_check_status(storage);
}

static int _sd_storage_set_hs25_bus_speed(sdmmc_storage_t *storage)
{
	sd_func_modes_t fmodes;
	u8 *buf = storage->raw_ext_csd;

	if (sd_storage_get_fmodes(storage, buf, &fmodes))
		return 1;

	DPRINTF("[SD] access: %02X, power: %02X\n", fmodes.access_mode, fmodes.power_limit);

	// No support, return success.
	if (!(fmodes.access_mode & SD_MODE_HIGH_SPEED))
		return 0;

	if (_sd_storage_set_card_bus_speed(storage, HIGH_SPEED_BUS_SPEED, buf))
		return 1;

	if (_sdmmc_storage_check_status(storage))
		return 1;

	return sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_SD_HS25);
}

bool sd_storage_get_ddr200_support(sdmmc_storage_t *storage)
{
	// DDR200 CID reserved/SCR vendor (hex):
	// Samsung 0x1B: A 00000000
	// Longsys 0xAD: A 00000000
	// Kowin   0x22: 7 33333039 33333039 SMART?
	// Phison  0x27: 0 01196432 Byte2: DDR2XX (25 == DDR225)?
	// Taishin 0x9F: 0 01006432

	// Non-DDR200:
	// Phison  0x27: 0 01000000
	// Taishin 0x9F: 0 01000000
	// Taishin 0x9F: 7 33333039 Supported but slow nand?

	sd_func_modes_t fmodes = { 0 };

	if (sd_storage_get_fmodes(storage, storage->raw_ext_csd, &fmodes))
		return false;

	// Introduced first in 2018 via Sandisk Quickflow.
	if (storage->cid.year < 2018)
		return false;

	// No DDR200 if no SDR104.
	if (!(fmodes.access_mode & SD_MODE_UHS_SDR104))
		return false;

	// Can't enter DDR200 mode without Vendor Command System mode.
	if (!(fmodes.cmd_system & SD_CMD_SYSTEM_VND))
		return false;

	// Assume no support if 4.10 and lower. (Should be < 6?)
	if (sd_storage_get_scr_sda_ver(storage) < 5)
		return false;

	// Sandisk.
	if (storage->cid.manfid == 0x03)
	{
		// 0x80: DDR200, 0x85: DDR225, 0x86: DDR250? 0x87:DDR275?
		if (storage->cid.prv >= 0x80)
			return true;
		else
			return false;
	}

	// CID Reserved bits.
	if (storage->cid.rsvd == 0xA || // Samsung / Longsys.
		storage->cid.rsvd == 0x7)   // Taishin A / Kowin.
		return true;

	// SCR Vendor bits.
	u32 scr_vnd = storage->scr.vendor & 0xFFFF;
	if (scr_vnd == 0x6432) // Phison / Taishin B. "d2".
		return true;

	return false;
}

u32 sd_storage_get_ssr_au(sdmmc_storage_t *storage)
{
	u32 au_size = storage->ssr.uhs_au_size;

	if (!au_size)
		au_size = storage->ssr.au_size;

	if (au_size <= 10)
	{
		u32 shift = au_size;
		au_size = shift ? 8 : 0;
		au_size <<= shift;
	}
	else
	{
		switch (au_size)
		{
		case 11:
			au_size = 12288;
			break;
		case 12:
			au_size = 16384;
			break;
		case 13:
			au_size = 24576;
			break;
		case 14:
			au_size = 32768;
			break;
		case 15:
			au_size = 65536;
			break;
		}
	}

	return au_size;
}

static void _sd_storage_parse_ssr(sdmmc_storage_t *storage)
{
	// unstuff_bits supports only 4 u32 so break into 2 x u32x4 groups.
	u32 raw_ssr1[4]; // 511:384.
	u32 raw_ssr2[4]; // 383:256.

	memcpy(raw_ssr1, &storage->raw_ssr[0],  16);
	memcpy(raw_ssr2, &storage->raw_ssr[16], 16);

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	_sd_storage_debug_print_ssr(storage->raw_ssr);
#endif

	storage->ssr.bus_width = (unstuff_bits(raw_ssr1, 510, 2) & SD_BUS_WIDTH_4) ? 4 : 1;
	storage->ssr.protected_size = unstuff_bits(raw_ssr1, 448, 32);

	u32 speed_class = unstuff_bits(raw_ssr1, 440, 8);
	switch(speed_class)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		storage->ssr.speed_class = speed_class << 1;
		break;

	case 4:
		storage->ssr.speed_class = 10;
		break;

	default:
		storage->ssr.speed_class = speed_class;
		break;
	}
	storage->ssr.uhs_grade   = unstuff_bits(raw_ssr1, 396, 4);
	storage->ssr.video_class = unstuff_bits(raw_ssr1, 384, 8);
	storage->ssr.app_class   = unstuff_bits(raw_ssr2, 336, 4);

	storage->ssr.au_size     = unstuff_bits(raw_ssr1, 428, 4);
	storage->ssr.uhs_au_size = unstuff_bits(raw_ssr1, 392, 4);

	storage->ssr.perf_enhance = unstuff_bits(raw_ssr2, 328, 8);
}

int sd_storage_parse_perf_enhance(sdmmc_storage_t *storage, u8 fno, u8 page, u16 offset, u8 *buf)
{
	// Check status reg for support.
	storage->ser.cache = (storage->ssr.perf_enhance >> 2) & BIT(0);
	storage->ser.cmdq  = (storage->ssr.perf_enhance >> 3) & 0x1F;

	if (sd_storage_get_ext_reg(storage, fno, page, offset, 512, buf))
	{
		storage->ser.cache_ext = 0;
		storage->ser.cmdq_ext  = 0;

		return 1;
	}

	storage->ser.cache_ext = buf[4] & BIT(0);
	storage->ser.cmdq_ext  = buf[6] & 0x1F;

	return 0;
}

static void _sd_storage_parse_ext_reg(sdmmc_storage_t *storage, u8 *buf, u16 *addr_next)
{
	u16 addr = *addr_next;

	// Address to the next extension.
	*addr_next = (buf[addr + 41] << 8) | buf[addr + 40];

	u16 sfc = (buf[addr + 1] << 8) | buf[addr];

	u32 reg_sets = buf[addr + 42];

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	for (u32 i = 0; i < reg_sets; i++)
	{
		u32 reg_set_addr;
		memcpy(&reg_set_addr, &buf[addr + 44 + 4 * i], 4);
		u16 off = reg_set_addr & 0x1FF;
		u8 page = reg_set_addr >> 9 & 0xFF;
		u8 fno  = reg_set_addr >> 18 & 0xFF;
		gfx_printf("Addr: %04X sfc:%02X - fno:%02X, page:%02X, off:%04X\n", addr, sfc, fno, page, off);
	}
#endif

	// Parse Performance Enhance.
	if (sfc == 2 && reg_sets == 1)
	{
		u32 reg_set0_addr;
		memcpy(&reg_set0_addr, &buf[addr + 44], 4);
		u16 off = reg_set0_addr & 0x1FF;
		u8 page = reg_set0_addr >> 9 & 0xFF;
		u8 fno  = reg_set0_addr >> 18 & 0xFF;

		if (sd_storage_parse_perf_enhance(storage, fno, page, off, buf))
			storage->ser.valid = 1;
	}
}

void sd_storage_get_ext_regs(sdmmc_storage_t *storage, u8 *buf)
{
	DREGPRINTF("SD Extension Registers:\n\n");

	if (!(storage->scr.cmds & BIT(2)))
	{
		DREGPRINTF("Not Supported!\n");
		return;
	}

	if (sd_storage_get_ext_reg(storage, 0, 0, 0, 512, buf))
	{
		DREGPRINTF("Failed to get general info!\n");
		return;
	}

	u16 size = (buf[3] << 8) | buf[2];
	u16 addr_next = 16;
	u32 num_ext = buf[4];
	for (u32 i = 0; i < num_ext && addr_next < size; i++)
		_sd_storage_parse_ext_reg(storage, buf, &addr_next);
}

int sd_storage_get_ssr(sdmmc_storage_t *storage)
{
	sdmmc_cmd_t cmdbuf;
	u8 *buf = storage->raw_ext_csd;

	sdmmc_init_cmd(&cmdbuf, SD_APP_SD_STATUS, 0, SDMMC_RSP_TYPE_1, 0);

	sdmmc_req_t reqbuf;
	reqbuf.buf              = buf;
	reqbuf.blksize          = SDMMC_CMD_BLOCKSIZE;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = 0;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (!(storage->csd.cmdclass & CCC_APP_SPEC))
	{
		DPRINTF("[SD] ssr: Not supported\n");
		return 1;
	}

	if (_sd_storage_execute_app_cmd(storage, R1_STATE_TRAN, 0, &cmdbuf, &reqbuf, NULL))
		return 1;

	// Convert buffer to LE.
	for (u32 i = 0; i < SDMMC_CMD_BLOCKSIZE; i += 4)
	{
		storage->raw_ssr[i + 3] = buf[i];
		storage->raw_ssr[i + 2] = buf[i + 1];
		storage->raw_ssr[i + 1] = buf[i + 2];
		storage->raw_ssr[i]     = buf[i + 3];
	}

	_sd_storage_parse_ssr(storage);

	return _sdmmc_storage_check_cached_card_status(storage->sdmmc);
}

static void _sd_storage_parse_cid(sdmmc_storage_t *storage)
{
	u32 *raw_cid = (u32 *)&(storage->raw_cid);

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	_sd_storage_debug_print_cid(raw_cid);
#endif

	storage->cid.manfid       = unstuff_bits(raw_cid, 120, 8);
	storage->cid.oemid        = unstuff_bits(raw_cid, 104, 16);
	storage->cid.prod_name[0] = unstuff_bits(raw_cid, 96,  8);
	storage->cid.prod_name[1] = unstuff_bits(raw_cid, 88,  8);
	storage->cid.prod_name[2] = unstuff_bits(raw_cid, 80,  8);
	storage->cid.prod_name[3] = unstuff_bits(raw_cid, 72,  8);
	storage->cid.prod_name[4] = unstuff_bits(raw_cid, 64,  8);
	storage->cid.hwrev        = unstuff_bits(raw_cid, 60,  4);
	storage->cid.fwrev        = unstuff_bits(raw_cid, 56,  4);
	storage->cid.serial       = unstuff_bits(raw_cid, 24,  32);
	storage->cid.rsvd         = unstuff_bits(raw_cid, 20,  4);
	storage->cid.year         = unstuff_bits(raw_cid, 12,  8) + 2000;
	storage->cid.month        = unstuff_bits(raw_cid, 8,   4);
}

static void _sd_storage_parse_csd(sdmmc_storage_t *storage)
{
	u32 *raw_csd = (u32 *)&(storage->raw_csd);

#ifdef SDMMC_DEBUG_PRINT_SD_REGS
	_sd_storage_debug_print_csd(raw_csd);
#endif

	storage->csd.structure     = unstuff_bits(raw_csd, 126, 2);
	storage->csd.cmdclass      = unstuff_bits(raw_csd, 84, 12);
	storage->csd.read_blkbits  = unstuff_bits(raw_csd, 80, 4);
	storage->csd.write_protect = unstuff_bits(raw_csd, 12, 2);
	switch(storage->csd.structure)
	{
	case 0:
		storage->csd.capacity = (1 + unstuff_bits(raw_csd, 62, 12)) << (unstuff_bits(raw_csd, 47, 3) + 2);
		storage->csd.capacity <<= unstuff_bits(raw_csd, 80, 4) - 9; // Convert native block size to LBA SDMMC_DAT_BLOCKSIZE.
		break;

	case 1:
		storage->csd.c_size       = (1 + unstuff_bits(raw_csd, 48, 22));
		storage->csd.capacity     = storage->csd.c_size << 10;
		storage->csd.read_blkbits = 9;
		break;

	default:
		DPRINTF("[SD] unknown CSD structure %d\n", storage->csd.structure);
		break;
	}

	storage->sec_cnt = storage->csd.capacity;
}

static bool _sd_storage_get_bus_uhs_support(u32 bus_width, u32 type)
{
	switch (type)
	{
	case SDHCI_TIMING_UHS_SDR12:
	case SDHCI_TIMING_UHS_SDR25:
	case SDHCI_TIMING_UHS_SDR50:
	case SDHCI_TIMING_UHS_SDR104:
	case SDHCI_TIMING_UHS_SDR82:
	case SDHCI_TIMING_UHS_DDR50:
	case SDHCI_TIMING_UHS_DDR200:
		if (bus_width == SDMMC_BUS_WIDTH_4)
			return true;
	default:
		return false;
	}
}

void sdmmc_storage_init_wait_sd()
{
	// T210/T210B01 WAR: Wait exactly 239ms for IO and Controller power to discharge.
	u32 sd_poweroff_time = (u32)get_tmr_ms() - sd_power_cycle_time_start;
	if (sd_poweroff_time < 239)
		msleep(239 - sd_poweroff_time);
}

int sdmmc_storage_init_sd(sdmmc_storage_t *storage, sdmmc_t *sdmmc, u32 bus_width, u32 type)
{
	bool is_sd_v1   = false;
	bool lv_support = _sd_storage_get_bus_uhs_support(bus_width, type);

	DPRINTF("[SD]-[init: bus: %d, type: %d]\n", bus_width, type);

	// Some cards (SanDisk U1), do not like a fast power cycle. Wait min 100ms.
	sdmmc_storage_init_wait_sd();

	memset(storage, 0, sizeof(sdmmc_storage_t));
	storage->sdmmc = sdmmc;

	if (sdmmc_init(sdmmc, SDMMC_1, SDMMC_POWER_3_3, SDMMC_BUS_WIDTH_1, SDHCI_TIMING_SD_ID))
		return 1;
	DPRINTF("[SD] after init\n");

	// Wait 1ms + 74 cycles.
	usleep(1000 + (74 * 1000 + sdmmc->card_clock - 1) / sdmmc->card_clock);

	if (_sdmmc_storage_go_idle_state(storage))
		return 1;
	DPRINTF("[SD] went to idle state\n");

	if (_sd_storage_send_if_cond(storage, &is_sd_v1))
		return 1;
	DPRINTF("[SD] sent if cond\n");

	if (_sd_storage_get_op_cond(storage, is_sd_v1, lv_support))
		return 1;
	DPRINTF("[SD] got op cond\n");

	if (_sdmmc_storage_get_cid(storage))
		return 1;
	DPRINTF("[SD] got cid\n");
	_sd_storage_parse_cid(storage);

	if (_sd_storage_get_rca(storage))
		return 1;
	DPRINTF("[SD] got rca (= %04X)\n", storage->rca);

	if (_sdmmc_storage_get_csd(storage))
		return 1;
	DPRINTF("[SD] got csd\n");
	_sd_storage_parse_csd(storage);

	if (!storage->is_low_voltage)
	{
		if (sdmmc_setup_clock(storage->sdmmc, SDHCI_TIMING_SD_DS12))
			return 1;
		DPRINTF("[SD] after setup default hs clock\n");
	}

	if (_sdmmc_storage_select_card(storage))
		return 1;
	DPRINTF("[SD] card selected\n");

	if (_sdmmc_storage_set_blocklen(storage, SD_BLOCKSIZE))
		return 1;
	DPRINTF("[SD] set blocklen to SD_BLOCKSIZE\n");

	// Disconnect Card Detect resistor from DAT3.
	if (_sd_storage_execute_app_cmd_tran(storage, SD_APP_SET_CLR_CARD_DETECT, 0))
		return 1;
	DPRINTF("[SD] cleared card detect\n");

	if (sd_storage_get_scr(storage))
		return 1;
	DPRINTF("[SD] got scr\n");

	// If card supports a wider bus and if it's not SD Version 1.0 switch bus width.
	if (bus_width == SDMMC_BUS_WIDTH_4 && (storage->scr.bus_widths & BIT(SD_BUS_WIDTH_4)) && storage->scr.sda_vsn)
	{
		if (_sd_storage_execute_app_cmd_tran(storage, SD_APP_SET_BUS_WIDTH, SD_BUS_WIDTH_4))
			return 1;

		sdmmc_set_bus_width(storage->sdmmc, SDMMC_BUS_WIDTH_4);
		DPRINTF("[SD] switched to wide bus width\n");
	}
	else
	{
		bus_width = SDMMC_BUS_WIDTH_1;
		DPRINTF("[SD] SD does not support wide bus width\n");
	}

	if (storage->is_low_voltage)
	{
		if (_sd_storage_set_uhs_bus_speed(storage, type))
			return 1;
		DPRINTF("[SD] enabled UHS\n");
	}
	else if (type != SDHCI_TIMING_SD_DS12 && storage->scr.sda_vsn) // Not default HS speed and not SD Version 1.0.
	{
		if (_sd_storage_set_hs25_bus_speed(storage))
			return 1;

		DPRINTF("[SD] enabled HS\n");
		switch (bus_width)
		{
		case SDMMC_BUS_WIDTH_4:
			storage->csd.busspeed = 25;
			break;

		case SDMMC_BUS_WIDTH_1:
			storage->csd.busspeed = 6;
			break;
		}
	}

	// Parse additional card info from sd status.
	if (!sd_storage_get_ssr(storage))
	{
		DPRINTF("[SD] got sd status\n");
	}

	sdmmc_card_clock_powersave(sdmmc, SDMMC_POWER_SAVE_ENABLE);

	storage->initialized = 1;

	return 0;
}

/*
 * Gamecard specific functions.
 */

int _gc_storage_custom_cmd(sdmmc_storage_t *storage, void *buf)
{
	u32 resp;
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_VENDOR_CMD_60, 0, SDMMC_RSP_TYPE_1, 1);

	sdmmc_req_t reqbuf;
	reqbuf.buf              = buf;
	reqbuf.blksize          = SDMMC_CMD_BLOCKSIZE;
	reqbuf.num_sectors      = 1;
	reqbuf.is_write         = 1;
	reqbuf.is_multi_block   = 0;
	reqbuf.is_auto_stop_trn = 0;

	if (sdmmc_execute_cmd(storage->sdmmc, &cmdbuf, &reqbuf, NULL))
	{
		sdmmc_stop_transmission(storage->sdmmc, &resp);
		return 1;
	}

	if (_sdmmc_storage_check_cached_card_status(storage->sdmmc))
		return 1;

	return _sdmmc_storage_check_status(storage);
}

int sdmmc_storage_init_gc(sdmmc_storage_t *storage, sdmmc_t *sdmmc)
{
	memset(storage, 0, sizeof(sdmmc_storage_t));
	storage->sdmmc = sdmmc;

	if (sdmmc_init(sdmmc, SDMMC_2, SDMMC_POWER_1_8, SDMMC_BUS_WIDTH_8, SDHCI_TIMING_MMC_HS100))
		return 1;
	DPRINTF("[GC] after init\n");

	// Wait 1ms + 10 clock cycles.
	usleep(1000 + (10 * 1000 + sdmmc->card_clock - 1) / sdmmc->card_clock);

	if (sdmmc_tuning_execute(storage->sdmmc, SDHCI_TIMING_MMC_HS100, MMC_SEND_TUNING_BLOCK_HS200))
		return 1;
	DPRINTF("[GC] after tuning\n");

	sdmmc_card_clock_powersave(sdmmc, SDMMC_POWER_SAVE_ENABLE);

	storage->initialized = 1;

	return 0;
}
