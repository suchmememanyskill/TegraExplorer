#include "keys.h"

#include "../config.h"
#include <gfx/di.h>
#include <gfx_utils.h>
#include "../hos/pkg1.h"
#include "../hos/pkg2.h"
#include "../hos/sept.h"
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <mem/mc.h>
#include <mem/minerva.h>
#include <mem/sdram.h>
#include <sec/se.h>
#include <sec/se_t210.h>
#include <sec/tsec.h>
#include <soc/fuse.h>
#include <mem/smmu.h>
#include <soc/t210.h>
#include "../storage/emummc.h"
#include "../storage/nx_emmc.h"
#include "../storage/nx_emmc_bis.h"
#include <storage/nx_sd.h>
#include <storage/sdmmc.h>
#include <utils/btn.h>
#include <utils/list.h>
#include <utils/sprintf.h>
#include <utils/util.h>
#include "../gfx/gfx.h"
#include "../tegraexplorer/tconf.h"
#include "../storage/mountmanager.h"

#include "key_sources.inl"

#include <string.h>

extern hekate_config h_cfg;

#define DPRINTF(x)

static int  _key_exists(const void *data) { return memcmp(data, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) != 0; };

static ALWAYS_INLINE u8 *_find_tsec_fw(const u8 *pkg1) {
    const u32 tsec_fw_align = 0x100;
    const u32 tsec_fw_first_instruction = 0xCF42004D;

    for (const u32 *pos = (const u32 *)pkg1; (u8 *)pos < pkg1 + PKG1_MAX_SIZE; pos += tsec_fw_align / sizeof(u32))
        if (*pos == tsec_fw_first_instruction)
            return (u8 *)pos;

    return NULL;
}

static ALWAYS_INLINE u32 _get_tsec_fw_size(tsec_key_data_t *key_data) {
    return key_data->blob0_size + sizeof(tsec_key_data_t) + key_data->blob1_size + key_data->blob2_size + key_data->blob3_size + key_data->blob4_size;
}

static void _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed) {
    if (!_key_exists(key_source) || !_key_exists(master_key) || !_key_exists(kek_seed))
        return;

    se_aes_key_set(ks, master_key, AES_128_KEY_SIZE);
    se_aes_unwrap_key(ks, ks, kek_seed);
    se_aes_unwrap_key(ks, ks, key_source);
    if (key_seed && _key_exists(key_seed))
        se_aes_unwrap_key(ks, ks, key_seed);
}

static void _get_device_key(u32 ks, void *out_device_key, u32 revision, const void *device_key, const void *new_device_key, const void *master_key) {
    if (revision == KB_FIRMWARE_VERSION_100_200 && !h_cfg.t210b01) {
        memcpy(out_device_key, device_key, AES_128_KEY_SIZE);
        return;
    }

    if (revision >= KB_FIRMWARE_VERSION_400) {
        revision -= KB_FIRMWARE_VERSION_400;
    } else {
        revision = 0;
    }
    u32 temp_key[AES_128_KEY_SIZE / 4] = {0};
    se_aes_key_set(ks, new_device_key, AES_128_KEY_SIZE);
    se_aes_crypt_ecb(ks, 0, temp_key, AES_128_KEY_SIZE, device_master_key_source_sources[revision], AES_128_KEY_SIZE);
    se_aes_key_set(ks, master_key, AES_128_KEY_SIZE);
    se_aes_unwrap_key(ks, ks, device_master_kek_sources[revision]);
    se_aes_crypt_ecb(ks, 0, out_device_key, AES_128_KEY_SIZE, temp_key, AES_128_KEY_SIZE);
}

static void _derive_misc_keys(key_derivation_ctx_t *keys) {
    if (_key_exists(keys->master_key)) {
        _generate_kek(8, header_kek_source, keys->master_key, aes_kek_generation_source, aes_key_generation_source);
        se_aes_crypt_block_ecb(8, 0, keys->header_key + 0x00, header_key_source + 0x00);
        se_aes_crypt_block_ecb(8, 0, keys->header_key + 0x10, header_key_source + 0x10);
    }

    if (_key_exists(keys->device_key) || (_key_exists(keys->master_key) && _key_exists(keys->device_key_4x))) {
        _get_device_key(8, keys->temp_key, 0, keys->device_key, keys->device_key_4x, keys->master_key);
        _generate_kek(8, save_mac_kek_source, keys->temp_key, aes_kek_generation_source, NULL);
        se_aes_crypt_block_ecb(8, 0, keys->save_mac_key, save_mac_key_source);
    }
}

static void _derive_bis_keys(key_derivation_ctx_t *keys) {
    /*  key = unwrap(source, wrapped_key):
        key_set(ks, wrapped_key), block_ecb(ks, 0, key, source) -> final key in key
    */
    minerva_periodic_training();
    u32 key_generation = fuse_read_odm_keygen_rev();
    if (key_generation)
        key_generation--;

    if (!(_key_exists(keys->device_key) || (key_generation && _key_exists(keys->master_key) && _key_exists(keys->device_key_4x)))) {
        return;
    }
    _get_device_key(8, keys->temp_key, key_generation, keys->device_key, keys->device_key_4x, keys->master_key);
    se_aes_key_set(8, keys->temp_key, AES_128_KEY_SIZE);
    se_aes_unwrap_key(8, 8, retail_specific_aes_key_source); // kek = unwrap(rsaks, devkey)
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[0] + 0x00, bis_key_source[0] + 0x00); // bkey = unwrap(bkeys, kek)
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[0] + 0x10, bis_key_source[0] + 0x10);
    // kek = generate_kek(bkeks, devkey, aeskek, aeskey)
    _generate_kek(8, bis_kek_source, keys->temp_key, aes_kek_generation_source, aes_key_generation_source);
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[1] + 0x00, bis_key_source[1] + 0x00); // bkey = unwrap(bkeys, kek)
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[1] + 0x10, bis_key_source[1] + 0x10);
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[2] + 0x00, bis_key_source[2] + 0x00);
    se_aes_crypt_block_ecb(8, 0, keys->bis_key[2] + 0x10, bis_key_source[2] + 0x10);
}

static int _derive_master_keys_from_keyblobs(key_derivation_ctx_t *keys) {
    u8 *keyblob_block = (u8 *)calloc(KB_FIRMWARE_VERSION_600 + 1, NX_EMMC_BLOCKSIZE);
    encrypted_keyblob_t *current_keyblob = (encrypted_keyblob_t *)keyblob_block;
    u32 keyblob_mac[AES_128_KEY_SIZE / 4] = {0};

    keys->sbk[0] = FUSE(FUSE_PRIVATE_KEY0);
    keys->sbk[1] = FUSE(FUSE_PRIVATE_KEY1);
    keys->sbk[2] = FUSE(FUSE_PRIVATE_KEY2);
    keys->sbk[3] = FUSE(FUSE_PRIVATE_KEY3);

    if (keys->sbk[0] == 0xFFFFFFFF) {
        u8 *aes_keys = (u8 *)calloc(0x1000, 1);
        se_get_aes_keys(aes_keys + 0x800, aes_keys, AES_128_KEY_SIZE);
        memcpy(keys->sbk, aes_keys + 14 * AES_128_KEY_SIZE, AES_128_KEY_SIZE);
        free(aes_keys);
    }

    se_aes_key_set(8, keys->tsec_keys, sizeof(keys->tsec_keys) / 2);
    se_aes_key_set(9, keys->sbk, 0x10);

    if (!emummc_storage_read(&emmc_storage, KEYBLOB_OFFSET / NX_EMMC_BLOCKSIZE, KB_FIRMWARE_VERSION_600 + 1, keyblob_block)) {
        DPRINTF("Unable to read keyblobs.");
    }

    se_aes_crypt_block_ecb(8, 0, keys->keyblob_key, keyblob_key_source); // temp = unwrap(kbks, tsec)
    se_aes_crypt_block_ecb(9, 0, keys->keyblob_key, keys->keyblob_key); // kbk = unwrap(temp, sbk)
    se_aes_key_set(7, keys->keyblob_key, sizeof(keys->keyblob_key));
    se_aes_crypt_block_ecb(7, 0, keys->keyblob_mac_key, keyblob_mac_key_source); // kbm = unwrap(kbms, kbk)
    se_aes_crypt_block_ecb(7, 0, keys->device_key, per_console_key_source); // devkey = unwrap(pcks, kbk0)
    se_aes_crypt_block_ecb(7, 0, keys->device_key_4x, device_master_key_source_kek_source);
    
    se_aes_key_set(10, keys->keyblob_mac_key, sizeof(keys->keyblob_mac_key));
    se_aes_cmac(10, keyblob_mac, sizeof(keyblob_mac), current_keyblob->iv, sizeof(current_keyblob->iv) + sizeof(keyblob_t));
    if (memcmp(current_keyblob, keyblob_mac, sizeof(keyblob_mac)) != 0) {
        //EPRINTFARGS("Keyblob %x corrupt.", 0);
        free(keyblob_block);
        return true;
    }

    se_aes_key_set(6, keys->keyblob_key, sizeof(keys->keyblob_key));
    se_aes_crypt_ctr(6, &keys->keyblob, sizeof(keyblob_t), &current_keyblob->key_data, sizeof(keyblob_t), current_keyblob->iv);

    memcpy(keys->package1_key, keys->keyblob.package1_key, sizeof(keys->package1_key));
    memcpy(keys->master_kek, keys->keyblob.master_kek, sizeof(keys->master_kek));
    se_aes_key_set(7, keys->master_kek, sizeof(keys->master_kek));
    se_aes_crypt_block_ecb(7, 0, keys->master_key, master_key_source);

    free(keyblob_block);
    return false;
}

static bool _derive_tsec_keys(tsec_ctxt_t *tsec_ctxt, u32 kb, key_derivation_ctx_t *keys) {
    tsec_ctxt->fw = _find_tsec_fw(tsec_ctxt->pkg1);
    if (!tsec_ctxt->fw) {
        DPRINTF("Unable to locate TSEC firmware.");
        return false;
    }

    minerva_periodic_training();

    tsec_ctxt->size = _get_tsec_fw_size((tsec_key_data_t *)(tsec_ctxt->fw + TSEC_KEY_DATA_OFFSET));
    if (tsec_ctxt->size > PKG1_MAX_SIZE) {
        DPRINTF("Unexpected TSEC firmware size.");
        return false;
    }

    int res = 0;
    u32 retries = 0;

    mc_disable_ahb_redirect();

    while (tsec_query(keys->tsec_keys, kb, tsec_ctxt) < 0) {
        memset(keys->tsec_keys, 0, sizeof(keys->tsec_keys));
        retries++;
        if (retries > 15) {
            res = -1;
            break;
        }
    }

    mc_enable_ahb_redirect();

    if (res < 0) {
        //EPRINTFARGS("ERROR %x dumping TSEC.\n", res);
        return false;
    }

    return true;
}

static ALWAYS_INLINE u8 *_read_pkg1(const pkg1_id_t **pkg1_id) {

    /*
    if (emummc_storage_init_mmc(&emmc_storage, &emmc_sdmmc)) {
        DPRINTF("Unable to init MMC.");
        return NULL;
    }
    */
    if (connectMMC(MMC_CONN_EMMC))
        return NULL;

    // Read package1.
    u8 *pkg1 = (u8 *)malloc(PKG1_MAX_SIZE);
    if (!emummc_storage_set_mmc_partition(&emmc_storage, EMMC_BOOT0)) {
        DPRINTF("Unable to set partition.");
        return NULL;
    }
    if (!emummc_storage_read(&emmc_storage, PKG1_OFFSET / NX_EMMC_BLOCKSIZE, PKG1_MAX_SIZE / NX_EMMC_BLOCKSIZE, pkg1)) {
        DPRINTF("Unable to read pkg1.");
        return NULL;
    }

    u32 pk1_offset = h_cfg.t210b01 ? sizeof(bl_hdr_t210b01_t) : 0; // Skip T210B01 OEM header.
    *pkg1_id = pkg1_identify(pkg1 + pk1_offset);
    if (!*pkg1_id) {
        DPRINTF("Unknown pkg1 version.\n Make sure you have the latest Lockpick_RCM.\n If a new firmware version just came out,\n Lockpick_RCM must be updated.\n Check Github for new release.");
        gfx_hexdump(0, pkg1, 0x20);
        return NULL;
    }

    return pkg1;
}

key_derivation_ctx_t __attribute__((aligned(4))) dumpedKeys = {0};

int DumpKeys(){
    if (h_cfg.t210b01) // i'm not even attempting to dump on mariko
        return 2;

    const pkg1_id_t *pkg1_id;
    u8 *pkg1 = _read_pkg1(&pkg1_id);
    if (!pkg1) {
        return 1;
    }

    TConf.pkg1ID = pkg1_id->id;
    TConf.pkg1ver = (u8)pkg1_id->kb;

    bool res = true;

    tsec_ctxt_t tsec_ctxt;
    tsec_ctxt.pkg1 = pkg1;
    res =_derive_tsec_keys(&tsec_ctxt, pkg1_id->kb, &dumpedKeys);
    
    free(pkg1);
    if (res == false) {
        return 1;
    }

    if (_derive_master_keys_from_keyblobs(&dumpedKeys))
        return 1;
    _derive_bis_keys(&dumpedKeys);
    _derive_misc_keys(&dumpedKeys);
    

    return 0;
}

void PrintKey(u8 *key, u32 len){
    for (int i = 0; i < len; i++){
        gfx_printf("%02x", key[i]);
    }
}