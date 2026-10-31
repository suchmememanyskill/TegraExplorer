/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2025 CTCaer
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

#ifndef _SDMMC_H_
#define _SDMMC_H_

#include <utils/types.h>
#include <storage/sd_def.h>
#include <storage/sdmmc_driver.h>

#define SDMMC_CMD_BLOCKSIZE 64
#define SDMMC_DAT_BLOCKSIZE 512
#define SDMMC_HMAX_BLOCKNUM 0xFFFF // HW max.
#define SDMMC_AMAX_BLOCKNUM 0xF000 // Aligned max.

extern u32 sd_power_cycle_time_start;

typedef enum _sdmmc_type
{
	MMC_SD   = 0,
	MMC_EMMC = 1,

	EMMC_GPP   = 0,
	EMMC_BOOT0 = 1,
	EMMC_BOOT1 = 2,
	EMMC_RPMB  = 3
} sdmmc_type;

typedef struct _mmc_sandisk_advanced_report_t
{
	u32 power_inits;

	u32 max_erase_cycles_sys;
	u32 max_erase_cycles_slc;
	u32 max_erase_cycles_mlc;

	u32 min_erase_cycles_sys;
	u32 min_erase_cycles_slc;
	u32 min_erase_cycles_mlc;

	u32 max_erase_cycles_euda;
	u32 min_erase_cycles_euda;
	u32 avg_erase_cycles_euda;
	u32 read_reclaim_cnt_euda;
	u32 bad_blocks_euda;

	u32 pre_eol_euda;
	u32 pre_eol_sys;
	u32 pre_eol_mlc;

	u32 uncorrectable_ecc;

	u32 temperature_now;
	u32 temperature_min;
	u32 temperature_max;

	u32 health_pct_euda;
	u32 health_pct_sys;
	u32 health_pct_mlc;

	u32 unk0;
	u32 unk1;
	u32 unk2;

	u32 reserved[78];
} mmc_sandisk_advanced_report_t;

typedef struct _mmc_sandisk_report_t
{
	u32 avg_erase_cycles_sys;
	u32 avg_erase_cycles_slc;
	u32 avg_erase_cycles_mlc;

	u32 read_reclaim_cnt_sys;
	u32 read_reclaim_cnt_slc;
	u32 read_reclaim_cnt_mlc;

	u32 bad_blocks_factory;
	u32 bad_blocks_sys;
	u32 bad_blocks_slc;
	u32 bad_blocks_mlc;

	u32 fw_updates_cnt;

	u8  fw_update_date[12];
	u8  fw_update_time[8];

	u32 total_writes_100mb;
	u32 vdrops;
	u32 vdroops;

	u32 vdrops_failed_data_rec;
	u32 vdrops_data_rec_ops;

	u32 total_writes_slc_100mb;
	u32 total_writes_mlc_100mb;

	u32 mlc_bigfile_mode_limit_exceeded;
	u32 avg_erase_cycles_hybrid;

	mmc_sandisk_advanced_report_t advanced;
} mmc_sandisk_report_t;

typedef struct _mmc_cid
{
	u32 manfid; // SDA assigned.
	u8  prod_name[8];
	u32 serial;
	u16 oemid;  // SDA assigned.
	u16	year;
	u8  prv;
	u8  hwrev;
	u8  fwrev;
	u8  month;
	u32 rsvd;
} mmc_cid_t;

typedef struct _mmc_csd
{
	u8  structure;
	u8  mmca_vsn;
	u16 cmdclass;
	u32 c_size;
	u32 r2w_factor;
	u32 read_blkbits;
	u32 capacity;
	u8  write_protect;
	u16 busspeed;
} mmc_csd_t;

typedef struct _mmc_ext_csd
{
	u8  bkops;        /* background support bit */
	u8  bkops_en;     /* manual bkops enable bit */
	u8  rev;
	u8  ext_struct;   /* 194 */
	u8  card_type;    /* 196 */
	u8  pre_eol_info;
	u8  dev_life_est_a;
	u8  dev_life_est_b;
	u8  boot_mult;
	u8  rpmb_mult;
	u16 dev_version;
	u32 cache_size;
	u32 max_enh_mult;
} mmc_ext_csd_t;

typedef struct _sd_scr
{
	u8 sda_vsn;
	u8 sda_spec;
	u8 bus_widths;
	u8 cmds;
	u32 vendor;
} sd_scr_t;

typedef struct _sd_ssr
{
	u8  bus_width;
	u8  speed_class;
	u8  uhs_grade;
	u8  video_class;
	u8  app_class;
	u8  au_size;
	u8  uhs_au_size;
	u8  perf_enhance;
	u32 protected_size;
} sd_ssr_t;

typedef struct _sd_ext_reg_t
{
	u8  cmdq;
	u8  cmdq_ext;
	u8  cache;
	u8  cache_ext;
	int valid;
} sd_ext_reg_t;

typedef struct _sd_func_modes_t
{
	u16 access_mode;
	u16 cmd_system;
	u16 driver_strength;
	u16 power_limit;
} sd_func_modes_t;

typedef struct _sd_vendor_info_t
{
	// CID Reserved.
	u8 cid_rsvd; // 4-bit.

	// CSD Reserved.
	u8 csd_rsvd8_9;     // 2-bit.
	u8 csd_rsvd16_20;   // 5-bit.
	u8 csd_rsvd29_30;   // 2-bit.
	u8 csd_rsvd120_125; // 6-bit.

	u32 scr_vendor;
	u8  scr_rsvd; // 2-bit.

	u32 ssr_vendor0_31;
	u32 ssr_vendor32_63;
	u32 ssr_vendor64_95;
	u32 ssr_vendor96_127;
	u32 ssr_vendor128_159;
	u32 ssr_vendor160_191;
	u32 ssr_vendor192_223;
	u32 ssr_vendor224_255;
	u32 ssr_vendor256_287;
	u32 ssr_vendor288_311; // 24-bit.

	u16 ssr_rsvd314_327; // 14-bit.
	u8  ssr_rsvd340_345; //  6-bit.
	u8  ssr_rsvd378_383; //  6-bit.
	u8  ssr_rsvd424_427; //  4-bit.
	u8  ssr_rsvd496_501; //  6-bit.
} sd_vendor_info_t;

/*! SDMMC storage context. */
typedef struct _sdmmc_storage_t
{
	sdmmc_t *sdmmc;

	int initialized;
	int is_low_voltage;
	int has_sector_access;
	int has_pcie;
	u32 rca;
	u32 sec_cnt;
	u32 partition;
	u32 max_power;
	u8  raw_cid[0x10]                    __attribute__((aligned(SDMMC_ADMA_ADDR_ALIGN)));
	u8  raw_csd[0x10]                    __attribute__((aligned(SDMMC_ADMA_ADDR_ALIGN)));
	u8  raw_scr[8]                       __attribute__((aligned(SDMMC_ADMA_ADDR_ALIGN)));
	u8  raw_ssr[SDMMC_CMD_BLOCKSIZE]     __attribute__((aligned(SDMMC_ADMA_ADDR_ALIGN)));
	u8  raw_ext_csd[SDMMC_DAT_BLOCKSIZE] __attribute__((aligned(SDMMC_ADMA_ADDR_ALIGN)));
	mmc_cid_t     cid;
	mmc_csd_t     csd;
	mmc_ext_csd_t ext_csd;
	sd_scr_t      scr;
	sd_ssr_t      ssr;
	sd_ext_reg_t  ser;
} sdmmc_storage_t;

int  sdmmc_storage_end(sdmmc_storage_t *storage);
int  sdmmc_storage_read(sdmmc_storage_t *storage, u32 sector, u32 num_sectors, void *buf);
int  sdmmc_storage_write(sdmmc_storage_t *storage, u32 sector, u32 num_sectors, void *buf);
int  sdmmc_storage_init_mmc(sdmmc_storage_t *storage, sdmmc_t *sdmmc, u32 bus_width, u32 type);
int  sdmmc_storage_set_mmc_partition(sdmmc_storage_t *storage, u32 partition);
void sdmmc_storage_init_wait_sd();
int  sdmmc_storage_init_sd(sdmmc_storage_t *storage, sdmmc_t *sdmmc, u32 bus_width, u32 type);
int  sdmmc_storage_init_gc(sdmmc_storage_t *storage, sdmmc_t *sdmmc);

int  sdmmc_storage_gen_cmd(sdmmc_storage_t *storage, u32 arg, void *buf);
int  sdmmc_storage_vendor_cmd(sdmmc_storage_t *storage, u32 arg);
int  sdmmc_storage_vendor_sandisk_report(sdmmc_storage_t *storage, void *buf);

int  mmc_storage_get_ext_csd(sdmmc_storage_t *storage);

int  sd_storage_get_ext_reg(sdmmc_storage_t *storage, u8 fno, u8 page, u16 offset, u32 len, void *buf);
int  sd_storage_get_fmodes(sdmmc_storage_t *storage, u8 *buf, sd_func_modes_t *functions);
int  sd_storage_get_scr(sdmmc_storage_t *storage);
u8   sd_storage_get_scr_sda_ver(sdmmc_storage_t *storage);
int  sd_storage_get_ssr(sdmmc_storage_t *storage);
u32  sd_storage_get_ssr_au(sdmmc_storage_t *storage);
void sd_storage_get_ext_regs(sdmmc_storage_t *storage, u8 *buf);
int  sd_storage_parse_perf_enhance(sdmmc_storage_t *storage, u8 fno, u8 page, u16 offset, u8 *buf);
bool sd_storage_get_ddr200_support(sdmmc_storage_t *storage);

void sd_storage_get_vendor_info(sdmmc_storage_t *storage, sd_vendor_info_t *info);

#endif
