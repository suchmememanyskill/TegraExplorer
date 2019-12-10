/*
 * Copyright (c) 2019 shchmue
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

#include "keys.h"

#include "../config/config.h"
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../gfx/tui.h"
#include "../hos/pkg1.h"
#include "../hos/pkg2.h"
#include "../hos/sept.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../mem/mc.h"
#include "../mem/minerva.h"
#include "../mem/sdram.h"
#include "../sec/se.h"
#include "../sec/se_t210.h"
#include "../sec/tsec.h"
#include "../soc/fuse.h"
#include "../soc/smmu.h"
#include "../soc/t210.h"
#include "../storage/emummc.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/list.h"
#include "../utils/sprintf.h"
#include "../utils/util.h"

#include "key_sources.inl"
#include "save.h"

#include <string.h>

extern bool sd_mount();
extern void sd_unmount();
extern int  sd_save_to_file(void *buf, u32 size, const char *filename);

extern hekate_config h_cfg;

extern bool clear_sector_cache;

u32 _key_count = 0, _titlekey_count = 0;
u32 color_idx = 0;
sdmmc_storage_t storage;
emmc_part_t *system_part;
u32 start_time, end_time;

#define TPRINTF(text) \
    end_time = get_tmr_us(); \
    gfx_printf(text" done in %d us\n", end_time - start_time); \
    start_time = get_tmr_us(); \
    minerva_periodic_training()

#define TPRINTFARGS(text, args...) \
    end_time = get_tmr_us(); \
    gfx_printf(text" done in %d us\n", args, end_time - start_time); \
    start_time = get_tmr_us(); \
    minerva_periodic_training()

#define SAVE_KEY(name, src, len) _save_key(name, src, len, text_buffer)
#define SAVE_KEY_FAMILY(name, src, start, count, len) _save_key_family(name, src, start, count, len, text_buffer)

// key functions
static bool  _key_exists(const void *data) { return memcmp(data, zeros, 0x10); };
static void  _save_key(const char *name, const void *data, u32 len, char *outbuf);
static void  _save_key_family(const char *name, const void *data, u32 start_key, u32 num_keys, u32 len, char *outbuf);
static void  _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed);
// nca functions
static void *_nca_process(u32 hk_ks1, u32 hk_ks2, FIL *fp, u32 key_offset, u32 len, const u8 key_area_key[3][KB_FIRMWARE_VERSION_MAX+1][0x10]);
static u32   _nca_fread_ctr(u32 ks, FIL *fp, void *buffer, u32 offset, u32 len, u8 *ctr);
static void  _update_ctr(u8 *ctr, u32 ofs);
// titlekey functions
static bool  _test_key_pair(const void *E, const void *D, const void *N);
static void  _mgf1_xor(void *masked, u32 masked_size, const void *seed, u32 seed_size);

void dump_keys() {
    u8  temp_key[0x10],
        bis_key[4][0x20] = {0},
        device_key[0x10] = {0},
        new_device_key[0x10] = {0},
        sd_seed[0x10] = {0},
        // FS-related keys
        fs_keys[13][0x20] = {0},
        header_key[0x20] = {0},
        save_mac_key[0x10] = {0},
        // other sysmodule sources
        es_keys[3][0x10] = {0},
        eticket_rsa_kek[0x10] = {0},
        ssl_keys[0x10] = {0},
        ssl_rsa_kek[0x10] = {0},
        // keyblob-derived families
        keyblob[KB_FIRMWARE_VERSION_600+1][0x90] = {0},
        keyblob_key[KB_FIRMWARE_VERSION_600+1][0x10] = {0},
        keyblob_mac_key[KB_FIRMWARE_VERSION_600+1][0x10] = {0},
        package1_key[KB_FIRMWARE_VERSION_600+1][0x10] = {0},
        // master key-derived families
        key_area_key[3][KB_FIRMWARE_VERSION_MAX+1][0x10] = {0},
        master_kek[KB_FIRMWARE_VERSION_MAX+1][0x10] = {0},
        master_key[KB_FIRMWARE_VERSION_MAX+1][0x10] = {0},
        package2_key[KB_FIRMWARE_VERSION_MAX+1][0x10] = {0},
        titlekek[KB_FIRMWARE_VERSION_MAX+1][0x10] = {0};

    sd_mount();

    display_backlight_brightness(h_cfg.backlight, 1000);
    gfx_clear_partial_grey(0x1B, 0, 1256);
    gfx_con_setpos(0, 0);

    gfx_printf("[%kLo%kck%kpi%kck%k_R%kCM%k v%d.%d.%d%k]\n\n",
        colors[0], colors[1], colors[2], colors[3], colors[4], colors[5], 0xFFFF00FF, LP_VER_MJ, LP_VER_MN, LP_VER_BF, 0xFFCCCCCC);

    tui_sbar(true);

    _key_count = 0;
    _titlekey_count = 0;
    color_idx = 0;

    start_time = get_tmr_us();
    u32 begin_time = get_tmr_us();
    u32 retries = 0;

    tsec_ctxt_t tsec_ctxt;
    sdmmc_t sdmmc;

    if (!emummc_storage_init_mmc(&storage, &sdmmc)) {
        EPRINTF("Unable to init MMC.");
        goto out_wait;
    }
    TPRINTFARGS("%kMMC init...     ", colors[(color_idx++) % 6]);

    // Read package1.
    u8 *pkg1 = (u8 *)malloc(0x40000);
    emummc_storage_set_mmc_partition(&storage, 1);
    emummc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, pkg1);
    const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
    if (!pkg1_id) {
        EPRINTF("Unknown pkg1 version.\n Make sure you have the latest Lockpick_RCM.\n If a new firmware version just came out,\n Lockpick_RCM must be updated.\n Check Github for new release.");
        goto out_wait;
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
        EPRINTF("Unable to locate TSEC firmware.");
        goto out_wait;
    }

    minerva_periodic_training();

    tsec_key_data_t *key_data = (tsec_key_data_t *)(tsec_ctxt.fw + TSEC_KEY_DATA_ADDR);
    tsec_ctxt.pkg1 = pkg1;
    tsec_ctxt.size = 0x100 + key_data->blob0_size + key_data->blob1_size + key_data->blob2_size + key_data->blob3_size + key_data->blob4_size;

    u32 MAX_KEY = 6;
    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_620) {
        MAX_KEY = pkg1_id->kb + 1;
    }

    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_700) {
        sd_mount();
        if (!f_stat("sd:/sept/payload.bak", NULL)) {
            if (f_unlink("sd:/sept/payload.bin"))
                gfx_printf("%kNote: no payload.bin already in /sept\n", colors[(color_idx++) % 6]);
            f_rename("sd:/sept/payload.bak", "sd:/sept/payload.bin");
        }

        if (!h_cfg.sept_run) {
            // bundle lp0 fw for sept instead of loading it from SD as hekate does
            sdram_lp0_save_params(sdram_get_params_patched());
            FIL fp;
            if (f_stat("sd:/sept", NULL)) {
                EPRINTF("On firmware 7.x+ but Sept missing.\nSkipping new key derivation...");
                goto get_tsec;
            }
            // backup post-reboot payload
            if (!f_stat("sd:/sept/payload.bin", NULL)) {
                if (f_rename("sd:/sept/payload.bin", "sd:/sept/payload.bak")) {
                    EPRINTF("Unable to backup payload.bin.");
                    goto out_wait;
                }
            }
            // write self to payload.bin to run again when sept finishes
            u32 payload_size = *(u32 *)(IPL_LOAD_ADDR + 0x84) - IPL_LOAD_ADDR;
            if (f_open(&fp, "sd:/sept/payload.bin", FA_CREATE_NEW | FA_WRITE)) {
                EPRINTF("Unable to open /sept/payload.bin to write.");
                goto out_wait;
            }
            if (f_write(&fp, (u8 *)IPL_LOAD_ADDR, payload_size, NULL)) {
                EPRINTF("Unable to write self to /sept/payload.bin.");
                f_close(&fp);
                goto out_wait;
            }
            f_close(&fp);
            gfx_printf("%k\nFirmware 7.x or higher detected.\n\n", colors[(color_idx++) % 6]);
            gfx_printf("%kRenamed /sept/payload.bin", colors[(color_idx++) % 6]);
            gfx_printf("\n     to /sept/payload.bak\n\n");
            gfx_printf("%kCopied self to /sept/payload.bin\n", colors[(color_idx++) % 6]);
            sdmmc_storage_end(&storage);
            if (!reboot_to_sept((u8 *)tsec_ctxt.fw, tsec_ctxt.size, pkg1_id->kb))
                goto out_wait;
        } else {
            se_aes_key_read(12, master_key[KB_FIRMWARE_VERSION_MAX], 0x10);
        }
    }

get_tsec: ;
    u8 tsec_keys[0x10 * 2] = {0};

    if (pkg1_id->kb == KB_FIRMWARE_VERSION_620) {
        u8 *tsec_paged = (u8 *)page_alloc(3);
        memcpy(tsec_paged, (void *)tsec_ctxt.fw, tsec_ctxt.size);
        tsec_ctxt.fw = tsec_paged;
    }

    int res = 0;

    mc_disable_ahb_redirect();

    while (tsec_query(tsec_keys, pkg1_id->kb, &tsec_ctxt) < 0) {
        memset(tsec_keys, 0x00, 0x20);
        retries++;
        if (retries > 15) {
            res = -1;
            break;
        }
    }
    free(pkg1);

    mc_enable_ahb_redirect();

    if (res < 0) {
        EPRINTFARGS("ERROR %x dumping TSEC.\n", res);
        goto out_wait;
    }

    TPRINTFARGS("%kTSEC key(s)...  ", colors[(color_idx++) % 6]);

    // Master key derivation
    if (pkg1_id->kb == KB_FIRMWARE_VERSION_620 && _key_exists(tsec_keys + 0x10)) {
        se_aes_key_set(8, tsec_keys + 0x10, 0x10); // mkek6 = unwrap(mkeks6, tsecroot)
        se_aes_crypt_block_ecb(8, 0, master_kek[6], master_kek_sources[0]);
        se_aes_key_set(8, master_kek[6], 0x10); // mkey = unwrap(mkek, mks)
        se_aes_crypt_block_ecb(8, 0, master_key[6], master_key_source);
    }

    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_620) {
        // derive all lower master keys in case keyblobs are bad
        if (_key_exists(master_key[pkg1_id->kb])) {
            for (u32 i = pkg1_id->kb; i > 0; i--) {
                se_aes_key_set(8, master_key[i], 0x10);
                se_aes_crypt_block_ecb(8, 0, master_key[i-1], mkey_vectors[i]);
            }
            se_aes_key_set(8, master_key[0], 0x10);
            se_aes_crypt_block_ecb(8, 0, temp_key, mkey_vectors[0]);
            if (_key_exists(temp_key)) {
                EPRINTFARGS("Unable to derive master key. kb = %d.\n Put current sept files on SD and retry.", pkg1_id->kb);
                memset(master_key, 0, sizeof(master_key));
            }
        } else if (_key_exists(master_key[KB_FIRMWARE_VERSION_MAX])) {
            // handle sept version differences
            for (u32 kb = KB_FIRMWARE_VERSION_MAX; kb >= KB_FIRMWARE_VERSION_620; kb--) {
                for (u32 i = kb; i > 0; i--) {
                    se_aes_key_set(8, master_key[i], 0x10);
                    se_aes_crypt_block_ecb(8, 0, master_key[i-1], mkey_vectors[i]);
                }
                se_aes_key_set(8, master_key[0], 0x10);
                se_aes_crypt_block_ecb(8, 0, temp_key, mkey_vectors[0]);
                if (!_key_exists(temp_key)) {
                    break;
                }
                memcpy(master_key[kb-1], master_key[kb], 0x10);
                memcpy(master_key[kb], zeros, 0x10);
            }
            if (_key_exists(temp_key)) {
                EPRINTF("Unable to derive master key.");
                memset(master_key, 0, sizeof(master_key));
            }
        }
    }

    u8 *keyblob_block = (u8 *)calloc(NX_EMMC_BLOCKSIZE, 1);
    u8 keyblob_mac[0x10] = {0};
    u32 sbk[4] = {FUSE(FUSE_PRIVATE_KEY0), FUSE(FUSE_PRIVATE_KEY1),
                  FUSE(FUSE_PRIVATE_KEY2), FUSE(FUSE_PRIVATE_KEY3)};
    se_aes_key_set(8, tsec_keys, 0x10);
    se_aes_key_set(9, sbk, 0x10);
    for (u32 i = 0; i <= KB_FIRMWARE_VERSION_600; i++) {
        minerva_periodic_training();
        se_aes_crypt_block_ecb(8, 0, keyblob_key[i], keyblob_key_source[i]); // temp = unwrap(kbks, tsec)
        se_aes_crypt_block_ecb(9, 0, keyblob_key[i], keyblob_key[i]); // kbk = unwrap(temp, sbk)
        se_aes_key_set(7, keyblob_key[i], 0x10);
        se_aes_crypt_block_ecb(7, 0, keyblob_mac_key[i], keyblob_mac_key_source); // kbm = unwrap(kbms, kbk)
        if (i == 0) {
            se_aes_crypt_block_ecb(7, 0, device_key, per_console_key_source); // devkey = unwrap(pcks, kbk0)
            se_aes_crypt_block_ecb(7, 0, new_device_key, per_console_key_source_4x);
        }

        // verify keyblob is not corrupt
        emummc_storage_read(&storage, 0x180000 / NX_EMMC_BLOCKSIZE + i, 1, keyblob_block);
        se_aes_key_set(3, keyblob_mac_key[i], 0x10);
        se_aes_cmac(3, keyblob_mac, 0x10, keyblob_block + 0x10, 0xa0);
        if (memcmp(keyblob_block, keyblob_mac, 0x10)) {
            EPRINTFARGS("Keyblob %x corrupt.", i);
            gfx_hexdump(i, keyblob_block, 0x10);
            gfx_hexdump(i, keyblob_mac, 0x10);
            continue;
        }

        // decrypt keyblobs
        se_aes_key_set(2, keyblob_key[i], 0x10);
        se_aes_crypt_ctr(2, keyblob[i], 0x90, keyblob_block + 0x20, 0x90, keyblob_block + 0x10);

        memcpy(package1_key[i], keyblob[i] + 0x80, 0x10);
        memcpy(master_kek[i], keyblob[i], 0x10);
        se_aes_key_set(7, master_kek[i], 0x10);
        se_aes_crypt_block_ecb(7, 0, master_key[i], master_key_source);
    }
    free(keyblob_block);

    TPRINTFARGS("%kMaster keys...  ", colors[(color_idx++) % 6]);

    /*  key = unwrap(source, wrapped_key):
        key_set(ks, wrapped_key), block_ecb(ks, 0, key, source) -> final key in key
    */
    minerva_periodic_training();
    u32 key_generation = 0;
    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_500) {
        if ((fuse_read_odm(4) & 0x800) && fuse_read_odm(0) == 0x8E61ECAE && fuse_read_odm(1) == 0xF2BA3BB2) {
            key_generation = fuse_read_odm(2) & 0x1F;
        }
    }
    if (_key_exists(device_key)) {
        if (key_generation) {
            se_aes_key_set(8, new_device_key, 0x10);
            se_aes_crypt_block_ecb(8, 0, temp_key, new_device_key_sources[pkg1_id->kb - KB_FIRMWARE_VERSION_400]);
            se_aes_key_set(8, master_key[0], 0x10);
            se_aes_unwrap_key(8, 8, new_device_keygen_sources[pkg1_id->kb - KB_FIRMWARE_VERSION_400]);
            se_aes_crypt_block_ecb(8, 0, temp_key, temp_key);
        } else
            memcpy(temp_key, device_key, 0x10);
        se_aes_key_set(8, temp_key, 0x10);
        se_aes_unwrap_key(8, 8, retail_specific_aes_key_source); // kek = unwrap(rsaks, devkey)
        se_aes_crypt_block_ecb(8, 0, bis_key[0] + 0x00, bis_key_source[0] + 0x00); // bkey = unwrap(bkeys, kek)
        se_aes_crypt_block_ecb(8, 0, bis_key[0] + 0x10, bis_key_source[0] + 0x10);
        // kek = generate_kek(bkeks, devkey, aeskek, aeskey)
        _generate_kek(8, bis_kek_source, temp_key, aes_kek_generation_source, aes_key_generation_source);
        se_aes_crypt_block_ecb(8, 0, bis_key[1] + 0x00, bis_key_source[1] + 0x00); // bkey = unwrap(bkeys, kek)
        se_aes_crypt_block_ecb(8, 0, bis_key[1] + 0x10, bis_key_source[1] + 0x10);
        se_aes_crypt_block_ecb(8, 0, bis_key[2] + 0x00, bis_key_source[2] + 0x00);
        se_aes_crypt_block_ecb(8, 0, bis_key[2] + 0x10, bis_key_source[2] + 0x10);
        memcpy(bis_key[3], bis_key[2], 0x20);
    }

    // Dump package2.
    u8 *pkg2 = NULL;
    pkg2_kip1_info_t *ki = NULL;

    emummc_storage_set_mmc_partition(&storage, 0);
    // Parse eMMC GPT.
    LIST_INIT(gpt);
    nx_emmc_gpt_parse(&gpt, &storage);

    // Find package2 partition.
    emmc_part_t *pkg2_part = nx_emmc_part_find(&gpt, "BCPKG2-1-Normal-Main");
    if (!pkg2_part) {
        EPRINTF("Unable to locate Package2.");
        goto pkg2_done;
    }

    // Read in package2 header and get package2 real size.
    u8 *tmp = (u8 *)malloc(NX_EMMC_BLOCKSIZE);
    nx_emmc_part_read(&storage, pkg2_part, 0x4000 / NX_EMMC_BLOCKSIZE, 1, tmp);
    u32 *hdr_pkg2_raw = (u32 *)(tmp + 0x100);
    u32 pkg2_size = hdr_pkg2_raw[0] ^ hdr_pkg2_raw[2] ^ hdr_pkg2_raw[3];
    free(tmp);

    if (pkg2_size > 0x7FC000) {
        EPRINTF("Invalid Package2 header.");
        goto pkg2_done;
    }
    // Read in package2.
    u32 pkg2_size_aligned = ALIGN(pkg2_size, NX_EMMC_BLOCKSIZE);
    pkg2 = malloc(pkg2_size_aligned);
    nx_emmc_part_read(&storage, pkg2_part, 0x4000 / NX_EMMC_BLOCKSIZE, pkg2_size_aligned / NX_EMMC_BLOCKSIZE, pkg2);

    // Decrypt package2 and parse KIP1 blobs in INI1 section. Try all available key generations in case of pkg1/pkg2 mismatch.
    minerva_periodic_training();
    pkg2_hdr_t *pkg2_hdr;
    pkg2_hdr_t hdr;
    u32 pkg2_kb;
    for (pkg2_kb = 0; pkg2_kb < MAX_KEY; pkg2_kb++) {
        se_aes_key_set(8, master_key[pkg2_kb], 0x10);
        se_aes_unwrap_key(8, 8, package2_key_source);
        memcpy(&hdr, pkg2 + 0x100, sizeof(pkg2_hdr_t));
        se_aes_crypt_ctr(8, &hdr, sizeof(pkg2_hdr_t), &hdr, sizeof(pkg2_hdr_t), &hdr);
        if (hdr.magic == PKG2_MAGIC)
            break;
    }
    if (pkg2_kb == MAX_KEY) {
        EPRINTF("Unable to derive Package2 key.");
        goto pkg2_done;
    } else if (pkg2_kb != pkg1_id->kb)
        EPRINTFARGS("Warning! Package1-Package2 mismatch: %d, %d", pkg1_id->kb, pkg2_kb);

    pkg2_hdr = pkg2_decrypt(pkg2);
    if (!pkg2_hdr) {
        EPRINTF("Unable to decrypt Package2.");
        goto pkg2_done;
    }

    TPRINTFARGS("%kDecrypt pkg2... ", colors[(color_idx++) % 6]);

    LIST_INIT(kip1_info);
    bool new_pkg2;
    pkg2_parse_kips(&kip1_info, pkg2_hdr, &new_pkg2);
    LIST_FOREACH_ENTRY(pkg2_kip1_info_t, ki_tmp, &kip1_info, link) {
        if(ki_tmp->kip1->tid == 0x0100000000000000ULL) {
            ki = malloc(sizeof(pkg2_kip1_info_t));
            memcpy(ki, ki_tmp, sizeof(pkg2_kip1_info_t));
            break;
        }
    }
    LIST_FOREACH_SAFE(iter, &kip1_info)
        free(CONTAINER_OF(iter, pkg2_kip1_info_t, link));

    if (!ki) {
        EPRINTF("Unable to parse INI1.");
        goto pkg2_done;
    }

    pkg2_decompress_kip(ki, 2 | 4); // we only need .rodata and .data
    TPRINTFARGS("%kDecompress FS...", colors[(color_idx++) % 6]);

    u8 hash_index = 0;
    const u8 key_lengths[13] = {0x10, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x10, 0x20, 0x20};

    if (!memcmp(pkg1_id->id, "2016", 4)) {
        // 1.0.0 doesn't have SD keys at all and the first key isn't aligned with the rest
        memcpy(fs_keys[2], ki->kip1->data + ki->kip1->sections[0].size_comp + 0x1ae0e, 0x10);
        hash_index = 1;
    }

    u8 temp_hash[0x20];
    for (u32 i = ki->kip1->sections[0].size_comp + pkg1_id->key_info.start_offset; i < ki->size - 0x20; ) {
        minerva_periodic_training();
        se_calc_sha256(temp_hash, ki->kip1->data + i, key_lengths[pkg1_id->key_info.hash_order[hash_index]]);
        if (!memcmp(temp_hash, fs_hashes_sha256[pkg1_id->key_info.hash_order[hash_index]], 0x20)) {
            memcpy(fs_keys[pkg1_id->key_info.hash_order[hash_index]], ki->kip1->data + i, key_lengths[pkg1_id->key_info.hash_order[hash_index]]);
            i += key_lengths[pkg1_id->key_info.hash_order[hash_index]];
            if (hash_index == pkg1_id->key_info.hash_max - 1) {
                if (pkg1_id->key_info.hks_offset_is_from_end)
                    i = ki->size - pkg1_id->key_info.hks_offset;
                else
                    i = ki->size - (ki->kip1->sections[2].size_decomp - pkg1_id->key_info.hks_offset);
            } else if (hash_index == pkg1_id->key_info.hash_max) {
                break;
            }
            hash_index++;
        } else {
            i += pkg1_id->key_info.alignment;
        }
    }
pkg2_done:
    if (ki) {
        free(ki);
    }
    free(pkg2);

    u8 *rights_ids = NULL, *titlekeys = NULL;

    TPRINTFARGS("%kFS keys...      ", colors[(color_idx++) % 6]);

    if (_key_exists(fs_keys[0]) && _key_exists(fs_keys[1]) && _key_exists(master_key[0])) {
        _generate_kek(8, fs_keys[0], master_key[0], aes_kek_generation_source, aes_key_generation_source);
        se_aes_crypt_block_ecb(8, 0, header_key + 0x00, fs_keys[1] + 0x00);
        se_aes_crypt_block_ecb(8, 0, header_key + 0x10, fs_keys[1] + 0x10);
    }

    if (_key_exists(fs_keys[5]) && _key_exists(fs_keys[6]) && _key_exists(device_key)) {
        _generate_kek(8, fs_keys[5], device_key, aes_kek_generation_source, NULL);
        se_aes_crypt_block_ecb(8, 0, save_mac_key, fs_keys[6]);
    }

    if (_key_exists(master_key[MAX_KEY])) {
        MAX_KEY = KB_FIRMWARE_VERSION_MAX + 1;
    }
    for (u32 i = 0; i < MAX_KEY; i++) {
        if (!_key_exists(master_key[i]))
            continue;
        if (_key_exists(fs_keys[2]) && _key_exists(fs_keys[3]) && _key_exists(fs_keys[4])) {
            for (u32 j = 0; j < 3; j++) {
                _generate_kek(8, fs_keys[2 + j], master_key[i], aes_kek_generation_source, NULL);
                se_aes_crypt_block_ecb(8, 0, key_area_key[j][i], aes_key_generation_source);
            }
        }
        se_aes_key_set(8, master_key[i], 0x10);
        se_aes_crypt_block_ecb(8, 0, package2_key[i], package2_key_source);
        se_aes_crypt_block_ecb(8, 0, titlekek[i], titlekek_source);
    }

    if (!_key_exists(header_key) || !_key_exists(bis_key[2]))
    {
        EPRINTF("Missing FS keys. Skipping ES/SSL keys.");
        goto key_output;
    }

    se_aes_key_set(4, header_key + 0x00, 0x10);
    se_aes_key_set(5, header_key + 0x10, 0x10);
    se_aes_key_set(8, bis_key[2] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[2] + 0x10, 0x10);

    system_part = nx_emmc_part_find(&gpt, "SYSTEM");
    if (!system_part) {
        EPRINTF("Unable to locate System partition.");
        goto key_output;
    }
    __attribute__ ((aligned (16))) FATFS emmc_fs;
    if (f_mount(&emmc_fs, "emmc:", 1)) {
        EPRINTF("Unable to mount system partition.");
        goto key_output;
    }

    DIR dir;
    FILINFO fno;
    FIL fp;
    save_ctx_t *save_ctx = NULL;

    // sysmodule NCAs only ever have one section (exefs) so 0x600 is sufficient
    u8 *dec_header = (u8*)malloc(0x600);
    char path[100] = "emmc:/Contents/registered";
    u32 titles_found = 0, title_limit = 2, read_bytes = 0;
    if (!memcmp(pkg1_id->id, "2016", 4))
        title_limit = 1;
    u8 *temp_file = NULL;

    if (f_opendir(&dir, path)) {
        EPRINTF("Unable to open System:/Contents/registered.");
        goto dismount;
    }

    // prepopulate /Contents/registered in decrypted sector cache
    while (!f_readdir(&dir, &fno) && fno.fname[0]) {}
    f_closedir(&dir);

    if (f_opendir(&dir, path)) {
        EPRINTF("Unable to open System:/Contents/registered.");
        goto dismount;
    }

    path[25] = '/';
    while (!f_readdir(&dir, &fno) && fno.fname[0] && titles_found < title_limit) {
        minerva_periodic_training();
        memcpy(path + 26, fno.fname, 36);
        path[62] = 0;
        if (fno.fattrib & AM_DIR)
            memcpy(path + 62, "/00", 4);
        if (f_open(&fp, path, FA_READ | FA_OPEN_EXISTING)) continue;
        if (f_lseek(&fp, 0x200) || f_read(&fp, dec_header, 32, &read_bytes) || read_bytes != 32) {
            f_close(&fp);
            continue;
        }
        se_aes_xts_crypt(5, 4, 0, 1, dec_header + 0x200, dec_header, 32, 1);
        // es doesn't contain es key sources on 1.0.0
        if (memcmp(pkg1_id->id, "2016", 4) && *(u32*)(dec_header + 0x210) == 0x33 && dec_header[0x205] == 0) {
            u8 hash_order[3] = {0, 1, 2};
            if (pkg1_id->kb >= KB_FIRMWARE_VERSION_500) {
                hash_order[0] = 1;
                hash_order[1] = 0;
            }
            hash_index = 0;
            // decrypt only what is needed to locate needed keys
            temp_file = (u8*)_nca_process(5, 4, &fp, pkg1_id->key_info.es_offset, 0xc0, key_area_key);
            for (u32 i = 0; i <= 0xb0; ) {
                se_calc_sha256(temp_hash, temp_file + i, 0x10);
                if (!memcmp(temp_hash, es_hashes_sha256[hash_order[hash_index]], 0x10)) {
                    memcpy(es_keys[hash_order[hash_index]], temp_file + i, 0x10);
                    hash_index++;
                    if (hash_index == 3)
                        break;
                    i += 0x10;
                } else {
                    i++;
                }
            }
            free(temp_file);
            temp_file = NULL;
            titles_found++;
        } else if (*(u32*)(dec_header + 0x210) == 0x24 && dec_header[0x205] == 0) {
            temp_file = (u8*)_nca_process(5, 4, &fp, pkg1_id->key_info.ssl_offset, 0x70, key_area_key);
            for (u32 i = 0; i <= 0x60; i++) {
                se_calc_sha256(temp_hash, temp_file + i, 0x10);
                if (!memcmp(temp_hash, ssl_hashes_sha256[1], 0x10)) {
                    memcpy(ssl_keys, temp_file + i, 0x10);
                    // only get ssl_rsa_kek_source_x from SSL on 1.0.0
                    // we get it from ES on every other firmware
                    // and it's located oddly distant from ssl_rsa_kek_source_y on >= 6.0.0
                    if (!memcmp(pkg1_id->id, "2016", 4)) {
                        se_calc_sha256(temp_hash, temp_file + i + 0x10, 0x10);
                        if (!memcmp(temp_hash, ssl_hashes_sha256[0], 0x10))
                            memcpy(es_keys[2], temp_file + i + 0x10, 0x10);
                    }
                    break;
                }
            }
            free(temp_file);
            temp_file = NULL;
            titles_found++;
        }
        f_close(&fp);
    }
    f_closedir(&dir);
    free(dec_header);

    // derive eticket_rsa_kek and ssl_rsa_kek
    if (_key_exists(es_keys[0]) && _key_exists(es_keys[1]) && _key_exists(master_key[0])) {
        for (u32 i = 0; i < 0x10; i++)
            temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_03[i];
        _generate_kek(7, es_keys[1], master_key[0], temp_key, NULL);
        se_aes_crypt_block_ecb(7, 0, eticket_rsa_kek, es_keys[0]);
    }
    if (_key_exists(ssl_keys) && _key_exists(es_keys[2]) && _key_exists(master_key[0])) {
        for (u32 i = 0; i < 0x10; i++)
            temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_01[i];
        _generate_kek(7, es_keys[2], master_key[0], temp_key, NULL);
        se_aes_crypt_block_ecb(7, 0, ssl_rsa_kek, ssl_keys);
    }

    if (memcmp(pkg1_id->id, "2016", 4)) {
        TPRINTFARGS("%kES & SSL keys...", colors[(color_idx++) % 6]);
    } else {
        TPRINTFARGS("%kSSL keys...     ", colors[(color_idx++) % 6]);
    }

    char private_path[200] = "sd:/";
    if (emu_cfg.nintendo_path && (emu_cfg.enabled || !h_cfg.emummc_force_disable)) {
        strcat(private_path, emu_cfg.nintendo_path);
    } else {
        strcat(private_path, "Nintendo");
    }
    strcat(private_path, "/Contents/private");
    if (f_open(&fp, private_path, FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Unable to open SD seed vector. Skipping.");
        goto get_titlekeys;
    }
    // get sd seed verification vector
    if (f_read(&fp, temp_key, 0x10, &read_bytes) || read_bytes != 0x10) {
        EPRINTF("Unable to read SD seed vector. Skipping.");
        f_close(&fp);
        goto get_titlekeys;
    }
    f_close(&fp);

    // this file is so small that parsing the savedata properly would take longer
    if (f_open(&fp, "emmc:/save/8000000000000043", FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Unable to open ns_appman save.\nSkipping SD seed.");
        goto get_titlekeys;
    }

    u8 read_buf[0x20] = {0};
    for (u32 i = 0x8000; i < f_size(&fp); i += 0x4000) {
        if (f_lseek(&fp, i) || f_read(&fp, read_buf, 0x20, &read_bytes) || read_bytes != 0x20)
            break;
        if (!memcmp(temp_key, read_buf, 0x10)) {
            memcpy(sd_seed, read_buf + 0x10, 0x10);
            break;
        }
    }
    f_close(&fp);

    TPRINTFARGS("%kSD Seed...      ", colors[(color_idx++) % 6]);

get_titlekeys:
    if (!_key_exists(eticket_rsa_kek))
        goto dismount;

    gfx_printf("%kTitlekeys...     ", colors[(color_idx++) % 6]);
    u32 save_x = gfx_con.x, save_y = gfx_con.y;
    gfx_printf("\n%kCommon...       ", colors[color_idx % 6]);

    u8 null_hash[0x20] = {
        0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14, 0x9A, 0xFB, 0xF4, 0xC8, 0x99, 0x6F, 0xB9, 0x24,
        0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B, 0x93, 0x4C, 0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55};

    se_aes_key_set(8, bis_key[0] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[0] + 0x10, 0x10);

    u32 buf_size = 0x4000;
    u8 *buffer = (u8 *)malloc(buf_size);

    u8 keypair[0x230] = {0};

    emummc_storage_read(&storage, 0x4400 / NX_EMMC_BLOCKSIZE, 0x4000 / NX_EMMC_BLOCKSIZE, buffer);

    se_aes_xts_crypt(9, 8, 0, 0, buffer, buffer, 0x4000, 1);

    se_aes_key_set(8, bis_key[2] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[2] + 0x10, 0x10);

    if (*(u32 *)buffer != 0x304C4143) {
        EPRINTF("CAL0 magic not found. Check BIS key 0.");
        free(buffer);
        goto dismount;
    }

    se_aes_key_set(2, eticket_rsa_kek, 0x10);
    se_aes_crypt_ctr(2, keypair, 0x230, buffer + 0x38a0, 0x230, buffer + 0x3890);

    u8 *D = keypair, *N = keypair + 0x100, *E = keypair + 0x200;

    // Check public exponent is 0x10001 big endian
    if (E[0] != 0 || E[1] != 1 || E[2] != 0 || E[3] != 1) {
        EPRINTF("Invalid public exponent.");
        free(buffer);
        goto dismount;
    }

    if (!_test_key_pair(E, D, N)) {
        EPRINTF("Invalid keypair. Check eticket_rsa_kek.");
        free(buffer);
        goto dismount;
    }

    se_rsa_key_set(0, N, 0x100, D, 0x100);

    u32 br = buf_size;
    u32 file_tkey_count = 0;
    u64 total_br = 0;
    rights_ids = (u8 *)malloc(0x40000);
    titlekeys = (u8 *)malloc(0x40000);
    save_ctx = calloc(1, sizeof(save_ctx_t));
    u8 M[0x100];
    if (f_open(&fp, "emmc:/save/80000000000000E1", FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Unable to open ES save 1. Skipping.");
        free(buffer);
        goto dismount;
    }

    u32 pct = 0, last_pct = 0;
    tui_pbar(save_x, save_y, pct, COLOR_GREEN, 0xFF155500);

    save_ctx->file = &fp;
    save_ctx->tool_ctx.action = 0;
    memcpy(save_ctx->save_mac_key, save_mac_key, 0x10);
    save_process(save_ctx);

    char ticket_bin_path[SAVE_FS_LIST_MAX_NAME_LENGTH] = "/ticket.bin";
    char ticket_list_bin_path[SAVE_FS_LIST_MAX_NAME_LENGTH] = "/ticket_list.bin";
    allocation_table_storage_ctx_t fat_storage;
    save_fs_list_entry_t entry = {0, "", {0}, 0};
    if (!save_hierarchical_file_table_get_file_entry_by_path(&save_ctx->save_filesystem_core.file_table, ticket_list_bin_path, &entry)) {
        EPRINTF("Unable to locate ticket_list.bin in e1.");
        goto dismount;
    }
    save_open_fat_storage(&save_ctx->save_filesystem_core, &fat_storage, entry.value.save_file_info.start_block);
    while (br == buf_size && total_br < entry.value.save_file_info.length) {
        br = save_allocation_table_storage_read(&fat_storage, buffer, total_br, buf_size);
        if (buffer[0] == 0) break;
        total_br += br;
        minerva_periodic_training();
        for (u32 j = 0; j < buf_size; j += 0x20) {
            if (buffer[j] == 0xff && buffer[j+1] == 0xff && buffer[j+2] == 0xff && buffer[j+3] == 0xff) break;
            file_tkey_count++;
        }
    }
    if (!save_hierarchical_file_table_get_file_entry_by_path(&save_ctx->save_filesystem_core.file_table, ticket_bin_path, &entry)) {
        EPRINTF("Unable to locate ticket.bin in e1.");
        goto dismount;
    }
    save_open_fat_storage(&save_ctx->save_filesystem_core, &fat_storage, entry.value.save_file_info.start_block);
    total_br = 0;
    while (br == buf_size && total_br < entry.value.save_file_info.length) {
        br = save_allocation_table_storage_read(&fat_storage, buffer, total_br, buf_size);
        if (buffer[0] == 0) break;
        total_br += br;
        for (u32 j = 0; j < buf_size; j += 0x400) {
            pct = _titlekey_count * 100 / file_tkey_count;
            if (pct > last_pct && pct <= 100) {
                last_pct = pct;
                tui_pbar(save_x, save_y, pct, COLOR_GREEN, 0xFF155500);
            }
            minerva_periodic_training();
            if (buffer[j] == 4 && buffer[j+1] == 0 && buffer[j+2] == 1 && buffer[j+3] == 0) {
                memcpy(rights_ids + 0x10 * _titlekey_count, buffer + j + 0x2a0, 0x10);
                memcpy(titlekeys + 0x10 * _titlekey_count, buffer + j + 0x180, 0x10);
                _titlekey_count++;
            } else {
                break;
            }
        }
    }
    tui_pbar(save_x, save_y, 100, COLOR_GREEN, 0xFF155500);
    f_close(&fp);
    save_free_contexts(save_ctx);
    memset(save_ctx, 0, sizeof(save_ctx_t));
    memset(&fat_storage, 0, sizeof(allocation_table_storage_ctx_t));

    gfx_con_setpos(0, save_y);
    TPRINTFARGS("\n%kCommon...       ", colors[(color_idx++) % 6]);
    save_x = gfx_con.x + 16 * 17;
    save_y = gfx_con.y;
    gfx_printf("\n%kPersonalized... ", colors[color_idx % 6]);

    u32 common_titlekey_count = _titlekey_count;
    if (f_open(&fp, "emmc:/save/80000000000000E2", FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Unable to open ES save 2. Skipping.");
        free(buffer);
        goto dismount;
    }

    save_ctx->file = &fp;
    save_ctx->tool_ctx.action = 0;
    memcpy(save_ctx->save_mac_key, save_mac_key, 0x10);
    save_process(save_ctx);

    if (!save_hierarchical_file_table_get_file_entry_by_path(&save_ctx->save_filesystem_core.file_table, ticket_list_bin_path, &entry)) {
        EPRINTF("Unable to locate ticket_list.bin in e2.");
        goto dismount;
    }
    save_open_fat_storage(&save_ctx->save_filesystem_core, &fat_storage, entry.value.save_file_info.start_block);

    total_br = 0;
    file_tkey_count = 0;
    while (br == buf_size && total_br < entry.value.save_file_info.length) {
        br = save_allocation_table_storage_read(&fat_storage, buffer, total_br, buf_size);
        if (buffer[0] == 0) break;
        total_br += br;
        minerva_periodic_training();
        for (u32 j = 0; j < buf_size; j += 0x20) {
            if (buffer[j] == 0xff && buffer[j+1] == 0xff && buffer[j+2] == 0xff && buffer[j+3] == 0xff) break;
            file_tkey_count++;
        }
    }
    if (!save_hierarchical_file_table_get_file_entry_by_path(&save_ctx->save_filesystem_core.file_table, ticket_bin_path, &entry)) {
        EPRINTF("Unable to locate ticket.bin in e2.");
        goto dismount;
    }

    save_open_fat_storage(&save_ctx->save_filesystem_core, &fat_storage, entry.value.save_file_info.start_block);

    total_br = 0;
    pct = 0;
    last_pct = 0;
    while (br == buf_size && total_br < entry.value.save_file_info.length) {
        br = save_allocation_table_storage_read(&fat_storage, buffer, total_br, buf_size);
        if (buffer[0] == 0) break;
        total_br += br;
        for (u32 j = 0; j < buf_size; j += 0x400) {
            pct = (_titlekey_count - common_titlekey_count) * 100 / file_tkey_count;
            if (pct > last_pct && pct <= 100) {
                last_pct = pct;
                tui_pbar(save_x, save_y, pct, COLOR_GREEN, 0xFF155500);
            }
            minerva_periodic_training();
            if (buffer[j] == 4 && buffer[j+1] == 0 && buffer[j+2] == 1 && buffer[j+3] == 0) {
                memcpy(rights_ids + 0x10 * _titlekey_count, buffer + j + 0x2a0, 0x10);

                u8 *titlekey_block = buffer + j + 0x180;
                se_rsa_exp_mod(0, M, 0x100, titlekey_block, 0x100);
                u8 *salt = M + 1;
                u8 *db = M + 0x21;
                _mgf1_xor(salt, 0x20, db, 0xdf);
                _mgf1_xor(db, 0xdf, salt, 0x20);
                if (memcmp(db, null_hash, 0x20))
                    continue;
                memcpy(titlekeys + 0x10 * _titlekey_count, db + 0xcf, 0x10);
                _titlekey_count++;
            } else {
                break;
            }
        }
    }
    tui_pbar(save_x, save_y, 100, COLOR_GREEN, 0xFF155500);
    free(buffer);
    f_close(&fp);

    gfx_con_setpos(0, save_y);
    TPRINTFARGS("\n%kPersonalized... ", colors[(color_idx++) % 6]);
    gfx_printf("\n%k  Found %d titlekeys.\n", colors[(color_idx++) % 6], _titlekey_count);

dismount:;
    if (save_ctx) {
        save_free_contexts(save_ctx);
        free(save_ctx);
    }
    f_mount(NULL, "emmc:", 1);
    clear_sector_cache = true;
    nx_emmc_gpt_free(&gpt);

key_output: ;
    char *text_buffer = NULL;
    if (!sd_mount()) {
        EPRINTF("Unable to mount SD.");
        goto free_buffers;
    }
    u32 text_buffer_size = _titlekey_count * 68 < 0x3000 ? 0x3000 : _titlekey_count * 68 + 1;
    text_buffer = (char *)calloc(1, text_buffer_size);

    SAVE_KEY("aes_kek_generation_source", aes_kek_generation_source, 0x10);
    SAVE_KEY("aes_key_generation_source", aes_key_generation_source, 0x10);
    SAVE_KEY("bis_kek_source", bis_kek_source, 0x10);
    SAVE_KEY_FAMILY("bis_key", bis_key, 0, 4, 0x20);
    SAVE_KEY_FAMILY("bis_key_source", bis_key_source, 0, 3, 0x20);
    SAVE_KEY("device_key", device_key, 0x10);
    SAVE_KEY("eticket_rsa_kek", eticket_rsa_kek, 0x10);
    SAVE_KEY("eticket_rsa_kek_source", es_keys[0], 0x10);
    SAVE_KEY("eticket_rsa_kekek_source", es_keys[1], 0x10);
    SAVE_KEY("header_kek_source", fs_keys[0], 0x10);
    SAVE_KEY("header_key", header_key, 0x20);
    SAVE_KEY("header_key_source", fs_keys[1], 0x20);
    SAVE_KEY_FAMILY("key_area_key_application", key_area_key[0], 0, MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_application_source", fs_keys[2], 0x10);
    SAVE_KEY_FAMILY("key_area_key_ocean", key_area_key[1], 0, MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_ocean_source", fs_keys[3], 0x10);
    SAVE_KEY_FAMILY("key_area_key_system", key_area_key[2], 0, MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_system_source", fs_keys[4], 0x10);
    SAVE_KEY_FAMILY("keyblob", keyblob, 0, 6, 0x90);
    SAVE_KEY_FAMILY("keyblob_key", keyblob_key, 0, 6, 0x10);
    SAVE_KEY_FAMILY("keyblob_key_source", keyblob_key_source, 0, 6, 0x10);
    SAVE_KEY_FAMILY("keyblob_mac_key", keyblob_mac_key, 0, 6, 0x10);
    SAVE_KEY("keyblob_mac_key_source", keyblob_mac_key_source, 0x10);
    SAVE_KEY_FAMILY("master_kek", master_kek, 0, MAX_KEY, 0x10);
    SAVE_KEY_FAMILY("master_kek_source", master_kek_sources, KB_FIRMWARE_VERSION_620, sizeof(master_kek_sources) / 0x10, 0x10);
    SAVE_KEY_FAMILY("master_key", master_key, 0, MAX_KEY, 0x10);
    SAVE_KEY("master_key_source", master_key_source, 0x10);
    SAVE_KEY_FAMILY("package1_key", package1_key, 0, 6, 0x10);
    SAVE_KEY_FAMILY("package2_key", package2_key, 0, MAX_KEY, 0x10);
    SAVE_KEY("package2_key_source", package2_key_source, 0x10);
    SAVE_KEY("per_console_key_source", per_console_key_source, 0x10);
    SAVE_KEY("retail_specific_aes_key_source", retail_specific_aes_key_source, 0x10);
    for (u32 i = 0; i < 0x10; i++)
        temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_03[i];
    SAVE_KEY("rsa_oaep_kek_generation_source", temp_key, 0x10);
    for (u32 i = 0; i < 0x10; i++)
        temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_01[i];
    SAVE_KEY("rsa_private_kek_generation_source", temp_key, 0x10);
    SAVE_KEY("save_mac_kek_source", fs_keys[5], 0x10);
    SAVE_KEY("save_mac_key", save_mac_key, 0x10);
    SAVE_KEY("save_mac_key_source", fs_keys[6], 0x10);
    SAVE_KEY("save_mac_sd_card_kek_source", fs_keys[7], 0x10);
    SAVE_KEY("save_mac_sd_card_key_source", fs_keys[8], 0x10);
    SAVE_KEY("sd_card_custom_storage_key_source", fs_keys[9], 0x20);
    SAVE_KEY("sd_card_kek_source", fs_keys[10], 0x10);
    SAVE_KEY("sd_card_nca_key_source", fs_keys[11], 0x20);
    SAVE_KEY("sd_card_save_key_source", fs_keys[12], 0x20);
    SAVE_KEY("sd_seed", sd_seed, 0x10);
    SAVE_KEY("secure_boot_key", sbk, 0x10);
    SAVE_KEY("ssl_rsa_kek", ssl_rsa_kek, 0x10);
    SAVE_KEY("ssl_rsa_kek_source_x", es_keys[2], 0x10);
    SAVE_KEY("ssl_rsa_kek_source_y", ssl_keys, 0x10);
    SAVE_KEY_FAMILY("titlekek", titlekek, 0, MAX_KEY, 0x10);
    SAVE_KEY("titlekek_source", titlekek_source, 0x10);
    SAVE_KEY("tsec_key", tsec_keys, 0x10);
    if (pkg1_id->kb == KB_FIRMWARE_VERSION_620)
        SAVE_KEY("tsec_root_key", tsec_keys + 0x10, 0x10);

    //gfx_con.fntsz = 8; gfx_puts(text_buffer); gfx_con.fntsz = 16;

    end_time = get_tmr_us();
    gfx_printf("\n%k  Found %d keys.\n\n", colors[(color_idx++) % 6], _key_count);
    gfx_printf("%kLockpick totally done in %d us\n\n", colors[(color_idx++) % 6], end_time - begin_time);
    gfx_printf("%kFound through master_key_%02x.\n\n", colors[(color_idx++) % 6], MAX_KEY - 1);

    f_mkdir("sd:/switch");
    char keyfile_path[30] = "sd:/switch/";
    if (!(fuse_read_odm(4) & 3))
        sprintf(&keyfile_path[11], "prod.keys");
    else
        sprintf(&keyfile_path[11], "dev.keys");
    if (!sd_save_to_file(text_buffer, strlen(text_buffer), keyfile_path) && !f_stat(keyfile_path, &fno)) {
        gfx_printf("%kWrote %d bytes to %s\n", colors[(color_idx++) % 6], (u32)fno.fsize, keyfile_path);
    } else
        EPRINTF("Unable to save keys to SD.");

    if (_titlekey_count == 0)
        goto free_buffers;
    memset(text_buffer, 0, text_buffer_size);
    for (u32 i = 0; i < _titlekey_count; i++) {
        for (u32 j = 0; j < 0x10; j++)
            sprintf(&text_buffer[i * 68 + j * 2], "%02x", rights_ids[i * 0x10 + j]);
        sprintf(&text_buffer[i * 68 + 0x20], " = ");
        for (u32 j = 0; j < 0x10; j++)
            sprintf(&text_buffer[i * 68 + 0x23 + j * 2], "%02x", titlekeys[i * 0x10 + j]);
        sprintf(&text_buffer[i * 68 + 0x43], "\n");
    }
    sprintf(&keyfile_path[11], "title.keys");
    if (!sd_save_to_file(text_buffer, strlen(text_buffer), keyfile_path) && !f_stat(keyfile_path, &fno)) {
        gfx_printf("%kWrote %d bytes to %s\n", colors[(color_idx++) % 6], (u32)fno.fsize, keyfile_path);
    } else
        EPRINTF("Unable to save titlekeys to SD.");

free_buffers:
    free(rights_ids);
    free(titlekeys);
    free(text_buffer);

out_wait:
    h_cfg.emummc_force_disable = emummc_load_cfg();
    emummc_storage_end(&storage);
    gfx_printf("\n%kPress any key to return to the main menu.", colors[(color_idx) % 6], colors[(color_idx + 1) % 6], colors[(color_idx + 2) % 6]);
    btn_wait();
}

static void _save_key(const char *name, const void *data, u32 len, char *outbuf) {
    if (!_key_exists(data))
        return;
    u32 pos = strlen(outbuf);
    pos += sprintf(&outbuf[pos], "%s = ", name);
    for (u32 i = 0; i < len; i++)
        pos += sprintf(&outbuf[pos], "%02x", *(u8*)(data + i));
    sprintf(&outbuf[pos], "\n");
    _key_count++;
}

static void _save_key_family(const char *name, const void *data, u32 start_key, u32 num_keys, u32 len, char *outbuf) {
    char temp_name[0x40] = {0};
    for (u32 i = 0; i < num_keys; i++) {
        sprintf(temp_name, "%s_%02x", name, i + start_key);
        _save_key(temp_name, data + i * len, len, outbuf);
    }
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

static inline u32 _read_le_u32(const void *buffer, u32 offset) {
    return (*(u8*)(buffer + offset + 0)        ) |
           (*(u8*)(buffer + offset + 1) << 0x08) |
           (*(u8*)(buffer + offset + 2) << 0x10) |
           (*(u8*)(buffer + offset + 3) << 0x18);
}

static void *_nca_process(u32 hk_ks1, u32 hk_ks2, FIL *fp, u32 key_offset, u32 len, const u8 key_area_key[3][KB_FIRMWARE_VERSION_MAX+1][0x10]) {
    u32 read_bytes = 0, crypt_offset, read_size, num_files, string_table_size, rodata_offset;

    u8 *temp_file = (u8*)malloc(0x400),
        ctr[0x10] = {0};
    if (f_lseek(fp, 0x200) || f_read(fp, temp_file, 0x400, &read_bytes) || read_bytes != 0x400) {
        free(temp_file);
        return NULL;
    }
    se_aes_xts_crypt(hk_ks1, hk_ks2, 0, 1, temp_file, temp_file, 0x200, 2);
    // both 1.x and 2.x use master_key_00
    temp_file[0x20] -= temp_file[0x20] ? 1 : 0;
    // decrypt key area and load decrypted key area key
    se_aes_key_set(7, key_area_key[temp_file[7]][temp_file[0x20]], 0x10);
    se_aes_crypt_block_ecb(7, 0, temp_file + 0x120, temp_file + 0x120);
    se_aes_key_set(2, temp_file + 0x120, 0x10);
    for (u32 i = 0; i < 8; i++)
        ctr[i] = temp_file[0x347 - i];
    crypt_offset = _read_le_u32(temp_file, 0x40) * 0x200 + _read_le_u32(temp_file, 0x240);
    read_size = 0x10;
    _nca_fread_ctr(2, fp, temp_file, crypt_offset, read_size, ctr);
    num_files = _read_le_u32(temp_file, 4);
    string_table_size = _read_le_u32(temp_file, 8);
    if (!memcmp(temp_file + 0x10 + num_files * 0x18, "main.npdm", 9))
        crypt_offset += _read_le_u32(temp_file, 0x18);
    crypt_offset += 0x10 + num_files * 0x18 + string_table_size;
    read_size = 0x40;
    _nca_fread_ctr(2, fp, temp_file, crypt_offset, read_size, ctr);
    rodata_offset = _read_le_u32(temp_file, 0x20);

    void *buf = malloc(len);
    _nca_fread_ctr(2, fp, buf, crypt_offset + rodata_offset + key_offset, len, ctr);
    free(temp_file);

    return buf;
}

static u32 _nca_fread_ctr(u32 ks, FIL *fp, void *buffer, u32 offset, u32 len, u8 *ctr) {
    u32 br;
    if (f_lseek(fp, offset) || f_read(fp, buffer, len, &br) || br != len)
        return 0;
    _update_ctr(ctr, offset);

    if (offset % 0x10) {
        u8 *temp = (u8*)malloc(ALIGN(br + offset % 0x10, 0x10));
        memcpy(temp + offset % 0x10, buffer, br);
        se_aes_crypt_ctr(ks, temp, ALIGN(br + offset % 0x10, 0x10), temp, ALIGN(br + offset % 0x10, 0x10), ctr);
        memcpy(buffer, temp + offset % 0x10, br);
        free(temp);
        return br;
    }
    se_aes_crypt_ctr(ks, buffer, br, buffer, br, ctr);
    return br;
}

static void _update_ctr(u8 *ctr, u32 ofs) {
    ofs >>= 4;
    for (u32 i = 0; i < 4; i++, ofs >>= 8)
        ctr[0x10-i-1] = (u8)(ofs & 0xff);
}

static bool _test_key_pair(const void *E, const void *D, const void *N) {
    u8 X[0x100] = {0}, Y[0x100] = {0}, Z[0x100] = {0};

    // 0xCAFEBABE
    X[0xfc] = 0xca; X[0xfd] = 0xfe; X[0xfe] = 0xba; X[0xff] = 0xbe;
    se_rsa_key_set(0, N, 0x100, D, 0x100);
    se_rsa_exp_mod(0, Y, 0x100, X, 0x100);
    se_rsa_key_set(0, N, 0x100, E, 4);
    se_rsa_exp_mod(0, Z, 0x100, Y, 0x100);

    return !memcmp(X, Z, 0x100);
}

// _mgf1_xor() was derived from Atmosphère's calculate_mgf1_and_xor
static void _mgf1_xor(void *masked, u32 masked_size, const void *seed, u32 seed_size) {
    u8 cur_hash[0x20];
    u8 hash_buf[0xe4];

    u32 hash_buf_size = seed_size + 4;
    memcpy(hash_buf, seed, seed_size);
    u32 round_num = 0;

    u8 *p_out = (u8 *)masked;

    while (masked_size) {
        u32 cur_size = masked_size > 0x20 ? 0x20 : masked_size;

        for (u32 i = 0; i < 4; i++)
            hash_buf[seed_size + 3 - i] = (round_num >> (8 * i)) & 0xff;
        round_num++;

        se_calc_sha256(cur_hash, hash_buf, hash_buf_size);

        for (unsigned int i = 0; i < cur_size; i++) {
            *p_out ^= cur_hash[i];
            p_out++;
        }

        masked_size -= cur_size;
    }
}
