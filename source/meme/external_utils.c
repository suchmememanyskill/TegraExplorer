#include "external_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "utils.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"
#include "../soc/hw_init.h"
#include "../mem/emc.h"
#include "../mem/sdram.h"
#include "../soc/t210.h"
#include "../sec/se.h"
#include "../utils/types.h"
#include "../hos/pkg1.h"
#include "../storage/nx_emmc.h"
#include "../sec/tsec.h"
#include "../soc/t210.h"
#include "../soc/fuse.h"
#include "../mem/mc.h"

extern void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size);
extern boot_cfg_t b_cfg;
extern bool sd_mount();
extern void sd_unmount();
static bool   _key_exists(const void *data) { return memcmp(data, zeros, 0x10); };
static void   _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed);

sdmmc_storage_t storage;
emmc_part_t *system_part;
sdmmc_t sdmmc;

int launch_payload(char *path, bool update){
	if (!update) gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);
	if (!path)
		return 1;

	if (sd_mount()){
		FIL fp;
		if (f_open(&fp, path, FA_READ)){
			EPRINTF("Payload missing!\n");
			return 2;
		}

		void *buf;
		u32 size = f_size(&fp);

		if (size < 0x30000)
			buf = (void *)RCM_PAYLOAD_ADDR;
		else
			buf = (void *)COREBOOT_ADDR;

		if (f_read(&fp, buf, size, NULL)){
			f_close(&fp);

			return 3;
		}

		f_close(&fp);

		sd_unmount();

		if (size < 0x30000){
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else {
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;
		void (*update_ptr)() = (void *)RCM_PAYLOAD_ADDR;

		msleep(100);

		if (!update)
			(*ext_payload_ptr)();
		else {
			EMC(EMC_SCRATCH0) |= EMC_HEKA_UPD;
			(*update_ptr)();
		}
	}

	return 4;
}

void dump_biskeys(u8 bis_key[4][32]){
	u8 temp_key[0x10], device_key[0x10] = {0};
    tsec_ctxt_t tsec_ctxt;

	int retries = 0;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);

    // Read package1.
    u8 *pkg1 = (u8 *)malloc(0x40000);
    sdmmc_storage_set_mmc_partition(&storage, 1);
    sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, pkg1);
    const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
    if (!pkg1_id) {
        EPRINTF("Unknown pkg1 version.");
        return;
    }

    bool found_tsec_fw = false;
    for (const u32 *pos = (const u32 *)pkg1; (u8 *)pos < pkg1 + 0x40000; pos += 0x100 / sizeof(u32)) {
        if (*pos == 0xCF42004D) {
            tsec_ctxt.fw = (u8 *)pos;
            found_tsec_fw = true;
            break;
        }
    }
    if (!found_tsec_fw) {
        EPRINTF("Failed to locate TSEC firmware.");
        return;
    }

    u8 tsec_keys[0x10] = {0};

    tsec_key_data_t *key_data = (tsec_key_data_t *)(tsec_ctxt.fw + TSEC_KEY_DATA_ADDR);
    tsec_ctxt.pkg1 = pkg1;
    tsec_ctxt.size = 0x100 + key_data->blob0_size + key_data->blob1_size + key_data->blob2_size + key_data->blob3_size + key_data->blob4_size;
    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_700) {
        // Exit after TSEC key generation.
        *((vu16 *)((u32)tsec_ctxt.fw + 0x2DB5)) = 0x02F8;
    }

    int res = 0;

    mc_disable_ahb_redirect();

    while (tsec_query(tsec_keys, 0, &tsec_ctxt) < 0) {
        memset(tsec_keys, 0x00, 0x10);
        retries++;
        if (retries > 15) {
            res = -1;
            break;
        }
    }
    free(pkg1);

    mc_enable_ahb_redirect();

    if (res < 0) {
        gfx_printf("ERROR %x dumping TSEC.\n", res);
        return;
    }

    u32 sbk[4] = {FUSE(FUSE_PRIVATE_KEY0), FUSE(FUSE_PRIVATE_KEY1),
                  FUSE(FUSE_PRIVATE_KEY2), FUSE(FUSE_PRIVATE_KEY3)};
    se_aes_key_set(8, tsec_keys, 0x10);
    se_aes_key_set(9, sbk, 0x10);
    se_aes_crypt_block_ecb(8, 0, temp_key, keyblob_key_source);
    se_aes_crypt_block_ecb(9, 0, temp_key, temp_key);
    se_aes_key_set(7, temp_key, 0x10);
    se_aes_crypt_block_ecb(7, 0, device_key, per_console_key_source);

    /*  key = unwrap(source, wrapped_key):
        key_set(ks, wrapped_key), block_ecb(ks, 0, key, source) -> final key in key
    */
    if (_key_exists(device_key)) {
        se_aes_key_set(8, device_key, 0x10);
        se_aes_unwrap_key(8, 8, retail_specific_aes_key_source); // kek = unwrap(rsaks, devkey)
        se_aes_crypt_block_ecb(8, 0, bis_key[0] + 0x00, bis_key_source[0] + 0x00); // bkey = unwrap(bkeys, kek)
        se_aes_crypt_block_ecb(8, 0, bis_key[0] + 0x10, bis_key_source[0] + 0x10);
        // kek = generate_kek(bkeks, devkey, aeskek, aeskey)
        _generate_kek(8, bis_kek_source, device_key, aes_kek_generation_source, aes_key_generation_source);
        se_aes_crypt_block_ecb(8, 0, bis_key[1] + 0x00, bis_key_source[1] + 0x00); // bkey = unwrap(bkeys, kek)
        se_aes_crypt_block_ecb(8, 0, bis_key[1] + 0x10, bis_key_source[1] + 0x10);
        se_aes_crypt_block_ecb(8, 0, bis_key[2] + 0x00, bis_key_source[2] + 0x00);
        se_aes_crypt_block_ecb(8, 0, bis_key[2] + 0x10, bis_key_source[2] + 0x10);
        memcpy(bis_key[3], bis_key[2], 0x20);
    }

    sdmmc_storage_set_mmc_partition(&storage, 0);
    // Parse eMMC GPT.
    LIST_INIT(gpt);
    nx_emmc_gpt_parse(&gpt, &storage);

    /*
    char part_name[37] = "SYSTEM";

    // todo: menu selection for this

    u32 bis_key_index = 0;
    if (strcmp(part_name, "PRODINFOF") == 0)
        bis_key_index = 0;
    else if (strcmp(part_name, "SAFE") == 0)
        bis_key_index = 1;
    else if (strcmp(part_name, "SYSTEM") == 0)
        bis_key_index = 2;
    else if (strcmp(part_name, "USER") == 0)
        bis_key_index = 3;
    else {
        gfx_printf("Partition name %s unrecognized.", part_name);
        return;
    }
    */
    se_aes_key_set(8, bis_key[2] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[2] + 0x10, 0x10);

    system_part = nx_emmc_part_find(&gpt, "SYSTEM");
    if (!system_part) {
        gfx_printf("Failed to locate SYSTEM partition.");
        return;
    }

    __attribute__ ((aligned (16))) FATFS emmc_fs;
    if (f_mount(&emmc_fs, "emmc:", 1)) {
        gfx_printf("Mount failed.");
        return;
    }
    return;
}

static void _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed) {
    if (!_key_exists(key_source) || !_key_exists(master_key) || !_key_exists(kek_seed))
        return;

    se_aes_key_set(ks, master_key, 0x10);
    se_aes_unwrap_key(ks, ks, kek_seed);
    se_aes_unwrap_key(ks, ks, key_source);
    if (key_seed && _key_exists(key_seed))
        se_aes_unwrap_key(ks, ks, key_seed);
}