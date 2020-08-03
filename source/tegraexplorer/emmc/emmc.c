#include <string.h>
#include "emmc.h"
#include "../../mem/heap.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../hos/pkg1.h"
#include "../../storage/sdmmc.h"
#include "../../storage/nx_emmc.h"
#include "../../sec/tsec.h"
#include "../../soc/t210.h"
#include "../../soc/fuse.h"
#include "../../mem/mc.h"
#include "../../sec/se.h"
#include "../../soc/hw_init.h"
#include "../../mem/emc.h"
#include "../../mem/sdram.h"
#include "../../storage/emummc.h"
#include "../../config/config.h"
#include "../common/common.h"
#include "../gfx/gfxutils.h"
#include "../../utils/list.h"
#include "../../mem/heap.h"

sdmmc_storage_t storage;
emmc_part_t *system_part;
sdmmc_t sdmmc;
extern hekate_config h_cfg;
__attribute__ ((aligned (16))) FATFS emmc;
LIST_INIT(sys_gpt);
LIST_INIT(emu_gpt);

u8 bis_key[4][32];
pkg1_info pkg1inf = {-1, ""};
short currentlyMounted = -1;


static bool  _key_exists(const void *data) { return memcmp(data, zeros, 0x10); };

static void _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed) {
    if (!_key_exists(key_source) || !_key_exists(master_key) || !_key_exists(kek_seed))
        return;

    se_aes_key_set(ks, master_key, 0x10);
    se_aes_unwrap_key(ks, ks, kek_seed);
    se_aes_unwrap_key(ks, ks, key_source);
    if (key_seed && _key_exists(key_seed))
        se_aes_unwrap_key(ks, ks, key_seed);
}

void print_biskeys(){
    gfx_printf("Bis_Key_00\n");
    gfx_hexdump(0, bis_key[0], 32);
    gfx_printf("Bis_Key_01\n");
    gfx_hexdump(0, bis_key[1], 32);
    gfx_printf("Bis_Key_02 & 03\n");
    gfx_hexdump(0, bis_key[2], 32);
}

pkg1_info returnpkg1info(){
    return pkg1inf;
}

int connect_part(const char *partition){
    sdmmc_storage_set_mmc_partition(&storage, 0);

    system_part = nx_emmc_part_find(selectGpt(currentlyMounted), partition);
    if (!system_part) {
        gfx_errDisplay("connect_mmc_part", ERR_PART_NOT_FOUND, 0);
        return 1;
    }

    return 0;
}

int mount_mmc(const char *partition, const int biskeynumb){
    int res;
    f_unmount("emmc:");
    
    se_aes_key_set(8, bis_key[biskeynumb] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[biskeynumb] + 0x10, 0x10);

    if (connect_part(partition))
        return 1;

    if ((res = f_mount(&emmc, "emmc:", 1))) {
        gfx_errDisplay("mount_mmc", res, 0);
        return 1;
    } 

    return 0;
}

void connect_mmc(short mmctype){
    if (mmctype != currentlyMounted){
        disconnect_mmc();
        h_cfg.emummc_force_disable = 0;
        switch (mmctype){
            case SYSMMC:
                sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_BUS_WIDTH_8, SDHCI_TIMING_MMC_HS400);
                h_cfg.emummc_force_disable = 1;
                currentlyMounted = SYSMMC;
                break;
            case EMUMMC:
                if (emummc_storage_init_mmc(&storage, &sdmmc)){
                    h_cfg.emummc_force_disable = 0;
                    currentlyMounted = EMUMMC;
                }
                break;
        }
    }
}

void disconnect_mmc(){
    f_unmount("emmc:");
    switch (currentlyMounted){
        case SYSMMC:
            sdmmc_storage_end(&storage);
            break;
        case EMUMMC:
            emummc_storage_end(&storage);
            break;
    }
    currentlyMounted = -1;
}

int dump_biskeys(){
	u8 temp_key[0x10], device_key[0x10] = {0};
    tsec_ctxt_t tsec_ctxt;

	int retries = 0;

    connect_mmc(SYSMMC);

    // Read package1.
    u8 *pkg1 = (u8 *)malloc(0x40000);
    sdmmc_storage_set_mmc_partition(&storage, 1);
    sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, pkg1);
    strncpy(pkg1inf.id, pkg1 + 0x10, 14);
    const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
    if (!pkg1_id) {
        EPRINTF("Unknown pkg1 version.");
        return -1;
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
        return -1;
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
        return -1;
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

    //sdmmc_storage_set_mmc_partition(&storage, 0);
    //nx_emmc_gpt_parse(&sys_gpt, &storage);
    
    
    pkg1inf.ver = pkg1_id->kb;
    strcpy(pkg1inf.id, pkg1_id->id);
    return 0;
}

void dumpGpt(){
    connect_mmc(SYSMMC);
    sdmmc_storage_set_mmc_partition(&storage, 0);
    nx_emmc_gpt_parse(&sys_gpt, &storage);

    if (emu_cfg.enabled){
        connect_mmc(EMUMMC);
        emummc_storage_set_mmc_partition(&storage, 0);
        nx_emmc_gpt_parse(&emu_gpt, &storage);
    }
}

link_t *selectGpt(short mmcType){
    switch(mmcType){
        case SYSMMC:
            return &sys_gpt;
        case EMUMMC:
            return &emu_gpt;
    }
    return NULL;
}

int checkGptRules(char *in){
    for (int i = 0; gpt_fs_rules[i].name != NULL; i++){
        if (!strcmp(in, gpt_fs_rules[i].name))
            return gpt_fs_rules[i].property;
    }
    return 0;
}