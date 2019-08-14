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
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../hos/pkg1.h"
#include "../hos/pkg2.h"
#include "../hos/sept.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../mem/mc.h"
#include "../mem/sdram.h"
#include "../sec/se.h"
#include "../sec/se_t210.h"
#include "../sec/tsec.h"
#include "../soc/fuse.h"
#include "../soc/smmu.h"
#include "../soc/t210.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/list.h"
#include "../utils/sprintf.h"
#include "../utils/util.h"

#include "key_sources.inl"

#include <string.h>

extern bool sd_mount();
extern void sd_unmount();
extern int  sd_save_to_file(void *buf, u32 size, const char *filename);

u32 _key_count = 0;
sdmmc_storage_t storage;
emmc_part_t *system_part;

#define TPRINTF(text) \
    end_time = get_tmr_ms(); \
    gfx_printf(text" done @ %d.%03ds\n", (end_time - start_time) / 1000, (end_time - start_time) % 1000)
#define TPRINTFARGS(text, args...) \
    end_time = get_tmr_ms(); \
    gfx_printf(text" done @ %d.%03ds\n", args, (end_time - start_time) / 1000, (end_time - start_time) % 1000)
#define SAVE_KEY(name, src, len) _save_key(name, src, len, text_buffer)
#define SAVE_KEY_FAMILY(name, src, count, len) _save_key_family(name, src, count, len, text_buffer)

static u8 temp_key[0x10],
          bis_key[4][0x20] = {0},
          device_key[0x10] = {0},
          sd_seed[0x10] = {0},
          // FS-related keys
          fs_keys[10][0x20] = {0},
          header_key[0x20] = {0},
          save_mac_key[0x10] = {0},
          // other sysmodule sources
          es_keys[3][0x10] = {0},
          eticket_rsa_kek[0x10] = {0},
          ssl_keys[2][0x10] = {0},
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

static const u32 colors[6] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET};

// key functions
static bool   _key_exists(const void *data) { return memcmp(data, zeros, 0x10); };
static void   _save_key(const char *name, const void *data, const u32 len, char *outbuf);
static void   _save_key_family(const char *name, const void *data, const u32 num_keys, const u32 len, char *outbuf);
static void   _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed, const void *key_seed);
// nca functions
static void  *_nca_process(u32 hk_ks1, u32 hk_ks2, FIL *fp, u32 key_offset, u32 len);
static u32    _nca_fread_ctr(u32 ks, FIL *fp, void *buffer, u32 offset, u32 len, u8 *ctr);
static void   _update_ctr(u8 *ctr, u32 ofs);

void dump_keys() {
    display_backlight_brightness(100, 1000);
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);

    gfx_printf("[%kLo%kck%kpi%kck%k-R%kCM%k v%d.%d.%d%k]\n\n",
        colors[0], colors[1], colors[2], colors[3], colors[4], colors[5], 0xFFFF00FF, LP_VER_MJ, LP_VER_MN, LP_VER_BF, 0xFFCCCCCC);

    u32 start_time = get_tmr_ms(),
        end_time,
        retries = 0;

    tsec_ctxt_t tsec_ctxt;
    sdmmc_t sdmmc;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);

    // Read package1.
    u8 *pkg1 = (u8 *)malloc(0x40000);
    sdmmc_storage_set_mmc_partition(&storage, 1);
    sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, pkg1);
    const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
    if (!pkg1_id) {
        EPRINTF("Unknown pkg1 version.");
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
        EPRINTF("Failed to locate TSEC firmware.");
        goto out_wait;
    }

    tsec_key_data_t *key_data = (tsec_key_data_t *)(tsec_ctxt.fw + TSEC_KEY_DATA_ADDR);
    tsec_ctxt.pkg1 = pkg1;
    tsec_ctxt.size = 0x100 + key_data->blob0_size + key_data->blob1_size + key_data->blob2_size + key_data->blob3_size + key_data->blob4_size;

    u32 MAX_KEY = 6;
    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_620)
        MAX_KEY = pkg1_id->kb + 1;

    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_700) {
        if (!f_stat("sd:/sept/payload.bak", NULL)) {
            f_unlink("sd:/sept/payload.bin");
            f_rename("sd:/sept/payload.bak", "sd:/sept/payload.bin");
        }

        if (!(EMC(EMC_SCRATCH0) & EMC_SEPT_RUN)) {
            // bundle lp0 fw for sept instead of loading it from SD as hekate does
            sdram_lp0_save_params(sdram_get_params_patched());
            FIL fp;
            if (f_stat("sd:/sept", NULL)) {
                EPRINTF("On firmware 7.x+ but Sept missing.\nSkipping new key derivation...");
                goto get_tsec;
            }
            // backup post-reboot payload
            if (!f_stat("sd:/sept/payload.bin", NULL))
                f_rename("sd:/sept/payload.bin", "sd:/sept/payload.bak");
            // write self to payload.bin to run again when sept finishes
            f_open(&fp, "sd:/sept/payload.bin", FA_CREATE_NEW | FA_WRITE);
            u32 payload_size = *(u32 *)(IPL_LOAD_ADDR + 0x84) - IPL_LOAD_ADDR;
            f_write(&fp, (u8 *)IPL_LOAD_ADDR, payload_size, NULL);
            f_close(&fp);
            gfx_printf("%kFirmware 7.x or higher detected.\n%kRenamed /sept/payload.bin", colors[0], colors[1]);
            gfx_printf("\n%k     to /sept/payload.bak\n%kCopied self to /sept/payload.bin",colors[2], colors[3]);
            sdmmc_storage_end(&storage);
            if (!reboot_to_sept((u8 *)tsec_ctxt.fw, tsec_ctxt.size, pkg1_id->kb))
                goto out_wait;
        } else {
            se_aes_key_read(12, master_key[pkg1_id->kb], 0x10);
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

    TPRINTFARGS("%kTSEC key(s)...  ", colors[0]);

    // Master key derivation
    if (pkg1_id->kb == KB_FIRMWARE_VERSION_620 && _key_exists(tsec_keys + 0x10)) {
        se_aes_key_set(8, tsec_keys + 0x10, 0x10); // mkek6 = unwrap(mkeks6, tsecroot)
        se_aes_crypt_block_ecb(8, 0, master_kek[6], master_kek_sources[0]);
        se_aes_key_set(8, master_kek[6], 0x10); // mkey = unwrap(mkek, mks)
        se_aes_crypt_block_ecb(8, 0, master_key[6], master_key_source);
    }

    if (pkg1_id->kb >= KB_FIRMWARE_VERSION_620 && _key_exists(master_key[pkg1_id->kb])) {
        // derive all lower master keys in the event keyblobs are bad
        for (u32 i = pkg1_id->kb; i > 0; i--) {
            se_aes_key_set(8, master_key[i], 0x10);
            se_aes_crypt_block_ecb(8, 0, master_key[i-1], mkey_vectors[i]);
        }
    }

    u8 *keyblob_block = (u8 *)calloc(NX_EMMC_BLOCKSIZE, 1);
    u8 keyblob_mac[0x10] = {0};
    u32 sbk[4] = {FUSE(FUSE_PRIVATE_KEY0), FUSE(FUSE_PRIVATE_KEY1),
                  FUSE(FUSE_PRIVATE_KEY2), FUSE(FUSE_PRIVATE_KEY3)};
    se_aes_key_set(8, tsec_keys, 0x10);
    se_aes_key_set(9, sbk, 0x10);
    for (u32 i = 0; i <= KB_FIRMWARE_VERSION_600; i++) {
        se_aes_crypt_block_ecb(8, 0, keyblob_key[i], keyblob_key_source[i]); // temp = unwrap(kbks, tsec)
        se_aes_crypt_block_ecb(9, 0, keyblob_key[i], keyblob_key[i]); // kbk = unwrap(temp, sbk)
        se_aes_key_set(7, keyblob_key[i], 0x10);
        se_aes_crypt_block_ecb(7, 0, keyblob_mac_key[i], keyblob_mac_key_source); // kbm = unwrap(kbms, kbk)
        if (i == 0)
            se_aes_crypt_block_ecb(7, 0, device_key, per_console_key_source); // devkey = unwrap(pcks, kbk0)

        // verify keyblob is not corrupt
        sdmmc_storage_read(&storage, 0x180000 / NX_EMMC_BLOCKSIZE + i, 1, keyblob_block);
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

    TPRINTFARGS("%kMaster keys...  ", colors[1]);

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

    // Dump package2.
    u8 *pkg2 = NULL;
    pkg2_kip1_info_t *ki = NULL;

    sdmmc_storage_set_mmc_partition(&storage, 0);
    // Parse eMMC GPT.
    LIST_INIT(gpt);
    nx_emmc_gpt_parse(&gpt, &storage);
    
    // Find package2 partition.
    emmc_part_t *pkg2_part = nx_emmc_part_find(&gpt, "BCPKG2-1-Normal-Main");
    if (!pkg2_part) {
        EPRINTF("Failed to locate Package2.");
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
        EPRINTF("Failed to decrypt Package2.");
        goto pkg2_done;
    } else if (pkg2_kb != pkg1_id->kb)
        EPRINTF("Warning: Package1-Package2 mismatch.");
    pkg2_hdr = pkg2_decrypt(pkg2);

    TPRINTFARGS("%kDecrypt pkg2... ", colors[2]);

    LIST_INIT(kip1_info);
    pkg2_parse_kips(&kip1_info, pkg2_hdr);
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
        EPRINTF("Failed to parse INI1.");
        goto pkg2_done;
    }

    pkg2_decompress_kip(ki, 2 | 4); // we only need .rodata and .data
    TPRINTFARGS("%kDecompress FS...", colors[3]);

    u8  hash_index = 0, hash_max = 9, hash_order[10],
        key_lengths[10] = {0x10, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x20};
    u32 start_offset = 0, hks_offset_from_end = ki->kip1->sections[2].size_decomp, alignment = 1;

    // the FS keys appear in different orders
    if (!memcmp(pkg1_id->id, "2016", 4)) {
        // 1.0.0 doesn't have SD keys at all
        hash_max = 6;
        // the first key isn't aligned with the rest
        memcpy(fs_keys[2], ki->kip1->data + ki->kip1->sections[0].size_comp + 0x1ae0e, 0x10);
        hash_index = 1;
        start_offset = 0x1b517;
        hks_offset_from_end = 0x125bc2;
        alignment = 0x10;
        u8 temp[7] = {2, 3, 4, 0, 5, 6, 1};
        memcpy(hash_order, temp, 7);
    } else {
        // 2.0.0 - 8.0.0
        alignment = 0x40;
        switch (pkg1_id->kb) {
        case KB_FIRMWARE_VERSION_100_200:
            start_offset = 0x1d226;
            alignment = 0x10;
            hks_offset_from_end -= 0x26fe;
            break;
        case KB_FIRMWARE_VERSION_300:
            start_offset = 0x1ffa6;
            hks_offset_from_end -= 0x298b;
            break;
        case KB_FIRMWARE_VERSION_301:
            start_offset = 0x20026;
            hks_offset_from_end -= 0x29ab;
            break;
        case KB_FIRMWARE_VERSION_400:
            start_offset = 0x1c64c;
            hks_offset_from_end -= 0x37eb;
            break;
        case KB_FIRMWARE_VERSION_500:
            start_offset = 0x1f3b4;
            hks_offset_from_end -= 0x465b;
            alignment = 0x20;
            break;
        case KB_FIRMWARE_VERSION_600:
        case KB_FIRMWARE_VERSION_620:
            start_offset = 0x27350;
            hks_offset_from_end = 0x17ff5;
            alignment = 8;
            break;
        case KB_FIRMWARE_VERSION_700:
        case KB_FIRMWARE_VERSION_810:
            start_offset = 0x29c50;
            hks_offset_from_end -= 0x6a73;
            alignment = 8;
            break;
        }

        if (pkg1_id->kb <= KB_FIRMWARE_VERSION_500) {
            u8 temp[10] = {2, 3, 4, 0, 5, 7, 9, 8, 6, 1};
            memcpy(hash_order, temp, 10);
        } else {
            u8 temp[10] = {6, 5, 7, 2, 3, 4, 0, 9, 8, 1};
            memcpy(hash_order, temp, 10);
        }
    }

    u8 temp_hash[0x20];
    for (u32 i = ki->kip1->sections[0].size_comp + start_offset; i < ki->size - 0x20; ) {
        se_calc_sha256(temp_hash, ki->kip1->data + i, key_lengths[hash_order[hash_index]]);
        if (!memcmp(temp_hash, fs_hashes_sha256[hash_order[hash_index]], 0x20)) {
            memcpy(fs_keys[hash_order[hash_index]], ki->kip1->data + i, key_lengths[hash_order[hash_index]]);
            /*if (hash_index == hash_max) {
                TPRINTFARGS("%d: %x    end -%x", hash_index, (*(ki->kip1->data + i)), ki->size - i);
            } else {
                TPRINTFARGS("%d: %x rodata +%x", hash_index, (*(ki->kip1->data + i)), i - ki->kip1->sections[0].size_comp);
            }*/
            i += key_lengths[hash_order[hash_index]];
            if (hash_index == hash_max - 1) {
                i = ki->size - hks_offset_from_end;
            } else if (hash_index == hash_max) {
                break;
            }
            hash_index++;
        } else {
            i += alignment;
        }
    }

pkg2_done:
    free(pkg2);
    free(ki);

    TPRINTFARGS("%kFS keys...      ", colors[4]);

    if (_key_exists(fs_keys[0]) && _key_exists(fs_keys[1]) && _key_exists(master_key[0])) {
        _generate_kek(8, fs_keys[0], master_key[0], aes_kek_generation_source, aes_key_generation_source);
        se_aes_crypt_block_ecb(8, 0, header_key + 0x00, fs_keys[1] + 0x00);
        se_aes_crypt_block_ecb(8, 0, header_key + 0x10, fs_keys[1] + 0x10);
    }

    if (_key_exists(fs_keys[5]) && _key_exists(fs_keys[6]) && _key_exists(device_key)) {
        _generate_kek(8, fs_keys[5], device_key, aes_kek_generation_source, NULL);
        se_aes_crypt_block_ecb(8, 0, save_mac_key, fs_keys[6]);
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
        goto key_output;

    se_aes_key_set(4, header_key + 0x00, 0x10);
    se_aes_key_set(5, header_key + 0x10, 0x10);
    se_aes_key_set(8, bis_key[2] + 0x00, 0x10);
    se_aes_key_set(9, bis_key[2] + 0x10, 0x10);

    system_part = nx_emmc_part_find(&gpt, "SYSTEM");
    if (!system_part) {
        EPRINTF("Failed to locate System partition.");
        goto key_output;
    }
    __attribute__ ((aligned (16))) FATFS emmc_fs;
    if (f_mount(&emmc_fs, "emmc:", 1)) {
        EPRINTF("Mount failed.");
        goto key_output;
    }

    DIR dir;
    FILINFO fno;
    FIL fp;
    // sysmodule NCAs only ever have one section (exefs) so 0x600 is sufficient
    u8 *dec_header = (u8*)malloc(0x600);
    char path[100] = "emmc:/Contents/registered";
    u32 titles_found = 0, title_limit = 2, read_bytes = 0;
    if (!memcmp(pkg1_id->id, "2016", 4))
        title_limit = 1;
    u8 *temp_file = NULL;

    if (f_opendir(&dir, path)) {
        EPRINTF("Failed to open System:/Contents/registered.");
        goto dismount;
    }

    // prepopulate /Contents/registered in decrypted sector cache
    while (!f_readdir(&dir, &fno) && fno.fname[0]) {}
    f_closedir(&dir);

    if (f_opendir(&dir, path)) {
        EPRINTF("Failed to open System:/Contents/registered.");
        goto dismount;
    }

    path[25] = '/';
    start_offset = 0;

    while (!f_readdir(&dir, &fno) && fno.fname[0] && titles_found < title_limit) {
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
            // es (offset 0x210 is lower half of titleid, 0x205 == 0 means it's program nca, not meta)
            switch (pkg1_id->kb) {
            case KB_FIRMWARE_VERSION_100_200:
                start_offset = 0x557b;
                break;
            case KB_FIRMWARE_VERSION_300:
            case KB_FIRMWARE_VERSION_301:
                start_offset = 0x552d;
                break;
            case KB_FIRMWARE_VERSION_400:
                start_offset = 0x5382;
                break;
            case KB_FIRMWARE_VERSION_500:
                start_offset = 0x5a63;
                break;
            case KB_FIRMWARE_VERSION_600:
            case KB_FIRMWARE_VERSION_620:
                start_offset = 0x5674;
                break;
            case KB_FIRMWARE_VERSION_700:
            case KB_FIRMWARE_VERSION_810:
                start_offset = 0x5563;
                break;
            }
            hash_order[2] = 2;
            if (pkg1_id->kb < KB_FIRMWARE_VERSION_500) {
                hash_order[0] = 0;
                hash_order[1] = 1;
            } else {
                hash_order[0] = 1;
                hash_order[1] = 0;
            }
            hash_index = 0;
            // decrypt only what is needed to locate needed keys
            temp_file = (u8*)_nca_process(5, 4, &fp, start_offset, 0xc0);
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
            // ssl
            switch (pkg1_id->kb) {
            case KB_FIRMWARE_VERSION_100_200:
                start_offset = 0x3d41a;
                break;
            case KB_FIRMWARE_VERSION_300:
            case KB_FIRMWARE_VERSION_301:
                start_offset = 0x3cb81;
                break;
            case KB_FIRMWARE_VERSION_400:
                start_offset = 0x3711c;
                break;
            case KB_FIRMWARE_VERSION_500:
                start_offset = 0x37901;
                break;
            case KB_FIRMWARE_VERSION_600:
            case KB_FIRMWARE_VERSION_620:
                start_offset = 0x1d5be;
                break;
            case KB_FIRMWARE_VERSION_700:
            case KB_FIRMWARE_VERSION_810:
                start_offset = 0x1d437;
                break;
            }
            if (!memcmp(pkg1_id->id, "2016", 4))
                start_offset = 0x449dc;
            temp_file = (u8*)_nca_process(5, 4, &fp, start_offset, 0x70);
            for (u32 i = 0; i <= 0x60; i++) {
                se_calc_sha256(temp_hash, temp_file + i, 0x10);
                if (!memcmp(temp_hash, ssl_hashes_sha256[1], 0x10)) {
                    memcpy(ssl_keys[1], temp_file + i, 0x10);
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

    if (f_open(&fp, "sd:/Nintendo/Contents/private", FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Unable to locate SD seed. Skipping.");
        goto dismount;
    }
    // get sd seed verification vector
    if (f_read(&fp, temp_key, 0x10, &read_bytes) || read_bytes != 0x10) {
        EPRINTF("Unable to locate SD seed. Skipping.");
        f_close(&fp);
        goto dismount;
    }
    f_close(&fp);

    if (f_open(&fp, "emmc:/save/8000000000000043", FA_READ | FA_OPEN_EXISTING)) {
        EPRINTF("Failed to open ns_appman save.\nSkipping SD seed.");
        goto dismount;
    }

    // locate sd seed
    u8 read_buf[0x20] = {0};
    for (u32 i = 0; i < f_size(&fp); i += 0x4000) {
        if (f_lseek(&fp, i) || f_read(&fp, read_buf, 0x20, &read_bytes) || read_bytes != 0x20)
            break;
        if (!memcmp(temp_key, read_buf, 0x10)) {
            memcpy(sd_seed, read_buf + 0x10, 0x10);
            break;
        }
    }
    f_close(&fp);

dismount:
    f_mount(NULL, "emmc:", 1);
    nx_emmc_gpt_free(&gpt);
    sdmmc_storage_end(&storage);

    if (memcmp(pkg1_id->id, "2016", 4)) {
        TPRINTFARGS("%kES & SSL keys...", colors[5]);
    } else {
        TPRINTFARGS("%kSSL keys...     ", colors[5]);
    }

    // derive eticket_rsa_kek and ssl_rsa_kek
    if (_key_exists(es_keys[0]) && _key_exists(es_keys[1]) && _key_exists(master_key[0])) {
        for (u32 i = 0; i < 0x10; i++)
            temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_03[i];
        _generate_kek(8, es_keys[1], master_key[0], temp_key, NULL);
        se_aes_crypt_block_ecb(8, 0, eticket_rsa_kek, es_keys[0]);
    }
    if (_key_exists(ssl_keys[1]) && _key_exists(es_keys[2]) && _key_exists(master_key[0])) {
        for (u32 i = 0; i < 0x10; i++)
            temp_key[i] = aes_kek_generation_source[i] ^ aes_kek_seed_01[i];
        _generate_kek(8, es_keys[2], master_key[0], temp_key, NULL);
        se_aes_crypt_block_ecb(8, 0, ssl_rsa_kek, ssl_keys[1]);
    }

key_output: ;
    __attribute__ ((aligned (16))) char text_buffer[0x3000] = {0};

    SAVE_KEY("aes_kek_generation_source", aes_kek_generation_source, 0x10);
    SAVE_KEY("aes_key_generation_source", aes_key_generation_source, 0x10);
    SAVE_KEY("bis_kek_source", bis_kek_source, 0x10);
    SAVE_KEY_FAMILY("bis_key", bis_key, 4, 0x20);
    SAVE_KEY_FAMILY("bis_key_source", bis_key_source, 3, 0x20);
    SAVE_KEY("device_key", device_key, 0x10);
    SAVE_KEY("eticket_rsa_kek", eticket_rsa_kek, 0x10);
    SAVE_KEY("eticket_rsa_kek_source", es_keys[0], 0x10);
    SAVE_KEY("eticket_rsa_kekek_source", es_keys[1], 0x10);
    SAVE_KEY("header_kek_source", fs_keys[0], 0x10);
    SAVE_KEY("header_key", header_key, 0x20);
    SAVE_KEY("header_key_source", fs_keys[1], 0x20);
    SAVE_KEY_FAMILY("key_area_key_application", key_area_key[0], MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_application_source", fs_keys[2], 0x10);
    SAVE_KEY_FAMILY("key_area_key_ocean", key_area_key[1], MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_ocean_source", fs_keys[3], 0x10);
    SAVE_KEY_FAMILY("key_area_key_system", key_area_key[2], MAX_KEY, 0x10);
    SAVE_KEY("key_area_key_system_source", fs_keys[4], 0x10);
    SAVE_KEY_FAMILY("keyblob", keyblob, 6, 0x90);
    SAVE_KEY_FAMILY("keyblob_key", keyblob_key, 6, 0x10);
    SAVE_KEY_FAMILY("keyblob_key_source", keyblob_key_source, 6, 0x10);
    SAVE_KEY_FAMILY("keyblob_mac_key", keyblob_mac_key, 6, 0x10);
    SAVE_KEY("keyblob_mac_key_source", keyblob_mac_key_source, 0x10);
    SAVE_KEY_FAMILY("master_kek", master_kek, MAX_KEY, 0x10);
    SAVE_KEY("master_kek_source_06", master_kek_sources[0], 0x10);
    SAVE_KEY("master_kek_source_07", master_kek_sources[1], 0x10);
    SAVE_KEY("master_kek_source_08", master_kek_sources[2], 0x10);
    SAVE_KEY_FAMILY("master_key", master_key, MAX_KEY, 0x10);
    SAVE_KEY("master_key_source", master_key_source, 0x10);
    SAVE_KEY_FAMILY("package1_key", package1_key, 6, 0x10);
    SAVE_KEY_FAMILY("package2_key", package2_key, MAX_KEY, 0x10);
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
    SAVE_KEY("sd_card_kek_source", fs_keys[7], 0x10);
    SAVE_KEY("sd_card_nca_key_source", fs_keys[8], 0x20);
    SAVE_KEY("sd_card_save_key_source", fs_keys[9], 0x20);
    SAVE_KEY("sd_seed", sd_seed, 0x10);
    SAVE_KEY("secure_boot_key", sbk, 0x10);
    SAVE_KEY("ssl_rsa_kek", ssl_rsa_kek, 0x10);
    SAVE_KEY("ssl_rsa_kek_source_x", es_keys[2], 0x10);
    SAVE_KEY("ssl_rsa_kek_source_y", ssl_keys[1], 0x10);
    SAVE_KEY_FAMILY("titlekek", titlekek, MAX_KEY, 0x10);
    SAVE_KEY("titlekek_source", titlekek_source, 0x10);
    SAVE_KEY("tsec_key", tsec_keys, 0x10);
    if (pkg1_id->kb == KB_FIRMWARE_VERSION_620)
        SAVE_KEY("tsec_root_key", tsec_keys + 0x10, 0x10);

    //gfx_con.fntsz = 8; gfx_puts(text_buffer); gfx_con.fntsz = 16;

    TPRINTFARGS("\n%kFound %d keys.\n%kLockpick totally", colors[0], _key_count, colors[1]);

    f_mkdir("switch");
    char keyfile_path[30] = "sd:/switch/";
    if (!(fuse_read_odm(4) & 3))
        sprintf(&keyfile_path[11], "prod.keys");
    else
        sprintf(&keyfile_path[11], "dev.keys");
    if (!sd_save_to_file(text_buffer, strlen(text_buffer), keyfile_path) && !f_stat(keyfile_path, &fno)) {
        gfx_printf("%kWrote %d bytes to %s\n", colors[2], (u32)fno.fsize, keyfile_path);
    } else
        EPRINTF("Failed to save keys to SD.");
    sd_unmount();

out_wait:
    gfx_printf("\n%kVOL + -> Reboot to RCM\n%kVOL - -> Reboot normally\n%kPower -> Power off", colors[3], colors[4], colors[5]);

    u32 btn = btn_wait();
    if (btn & BTN_VOL_UP)
        reboot_rcm();
    else if (btn & BTN_VOL_DOWN)
        reboot_normal();
    else
        power_off();
}

static void _save_key(const char *name, const void *data, const u32 len, char *outbuf) {
    if (!_key_exists(data))
        return;
    u32 pos = strlen(outbuf);
    pos += sprintf(&outbuf[pos], "%s = ", name);
    for (u32 i = 0; i < len; i++)
        pos += sprintf(&outbuf[pos], "%02x", *(u8*)(data + i));
    sprintf(&outbuf[pos], "\n");
    _key_count++;
}

static void _save_key_family(const char *name, const void *data, const u32 num_keys, const u32 len, char *outbuf) {
    char temp_name[0x40] = {0};
    for (u32 i = 0; i < num_keys; i++) {
        sprintf(temp_name, "%s_%02x", name, i);
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

static void *_nca_process(u32 hk_ks1, u32 hk_ks2, FIL *fp, u32 key_offset, u32 len) {
    u32 read_bytes = 0, crypt_offset, read_size, num_files, string_table_size, rodata_offset;

    u8 *temp_file = (u8*)malloc(0x400),
        ctr[0x10] = {0};
    if (f_lseek(fp, 0x200) || f_read(fp, temp_file, 0x400, &read_bytes) || read_bytes != 0x400)
        return NULL;
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
