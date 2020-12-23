/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 CTCaer
 * Copyright (c) 2018 Atmosphère-NX
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

#include <string.h>

#include "se.h"
#include "se_t210.h"
#include <memory_map.h>
#include <mem/heap.h>
#include <soc/bpmp.h>
#include <soc/pmc.h>
#include <soc/t210.h>
#include <utils/util.h>

typedef struct _se_ll_t
{
	vu32 num;
	vu32 addr;
	vu32 size;
} se_ll_t;

static u32 _se_rsa_mod_sizes[TEGRA_SE_RSA_KEYSLOT_COUNT];
static u32 _se_rsa_exp_sizes[TEGRA_SE_RSA_KEYSLOT_COUNT];

static void _gf256_mul_x(void *block)
{
	u8 *pdata = (u8 *)block;
	u32 carry = 0;

	for (int i = 0xF; i >= 0; i--)
	{
		u8 b = pdata[i];
		pdata[i] = (b << 1) | carry;
		carry = b >> 7;
	}

	if (carry)
		pdata[0xF] ^= 0x87;
}

static void _gf256_mul_x_le(void *block)
{
	u32 *pdata = (u32 *)block;
	u32 carry = 0;

	for (u32 i = 0; i < 4; i++)
	{
		u32 b = pdata[i];
		pdata[i] = (b << 1) | carry;
		carry = b >> 31;
	}

	if (carry)
		pdata[0x0] ^= 0x87;
}

static void _se_ll_init(se_ll_t *ll, u32 addr, u32 size)
{
	ll->num = 0;
	ll->addr = addr;
	ll->size = size;
}

static void _se_ll_set(se_ll_t *dst, se_ll_t *src)
{
	SE(SE_IN_LL_ADDR_REG_OFFSET) = (u32)src;
	SE(SE_OUT_LL_ADDR_REG_OFFSET) = (u32)dst;
}

static int _se_wait()
{
	while (!(SE(SE_INT_STATUS_REG_OFFSET) & SE_INT_OP_DONE(INT_SET)))
		;
	if (SE(SE_INT_STATUS_REG_OFFSET) & SE_INT_ERROR(INT_SET) ||
		SE(SE_STATUS_0) & SE_STATUS_0_STATE_WAIT_IN ||
		SE(SE_ERR_STATUS_0) != SE_ERR_STATUS_0_SE_NS_ACCESS_CLEAR)
		return 0;
	return 1;
}

se_ll_t *ll_dst, *ll_src;
static int _se_execute(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size, bool is_oneshot)
{
	ll_dst = NULL;
	ll_src = NULL;

	if (dst)
	{
		ll_dst = (se_ll_t *)malloc(sizeof(se_ll_t));
		_se_ll_init(ll_dst, (u32)dst, dst_size);
	}

	if (src)
	{
		ll_src = (se_ll_t *)malloc(sizeof(se_ll_t));
		_se_ll_init(ll_src, (u32)src, src_size);
	}

	_se_ll_set(ll_dst, ll_src);

	SE(SE_ERR_STATUS_0) = SE(SE_ERR_STATUS_0);
	SE(SE_INT_STATUS_REG_OFFSET) = SE(SE_INT_STATUS_REG_OFFSET);

	bpmp_mmu_maintenance(BPMP_MMU_MAINT_CLN_INV_WAY, false);

	SE(SE_OPERATION_REG_OFFSET) = SE_OPERATION(op);

	if (is_oneshot)
	{
		int res = _se_wait();

		bpmp_mmu_maintenance(BPMP_MMU_MAINT_CLN_INV_WAY, false);

		if (src)
			free(ll_src);
		if (dst)
			free(ll_dst);

		return res;
	}

	return 1;
}

static int _se_execute_finalize()
{
	int res = _se_wait();

	bpmp_mmu_maintenance(BPMP_MMU_MAINT_CLN_INV_WAY, false);

	if (ll_src)
	{
		free(ll_src);
		ll_src = NULL;
	}
	if (ll_dst)
	{
		free(ll_dst);
		ll_dst = NULL;
	}

	return res;
}

static int _se_execute_oneshot(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	return _se_execute(op, dst, dst_size, src, src_size, true);
}

static int _se_execute_one_block(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	if (!src || !dst)
		return 0;

	u8 *block = (u8 *)malloc(0x10);
	memset(block, 0, 0x10);

	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;

	memcpy(block, src, src_size);
	int res = _se_execute_oneshot(op, block, 0x10, block, 0x10);
	memcpy(dst, block, dst_size);

	free(block);
	return res;
}

static void _se_aes_ctr_set(void *ctr)
{
	u32 data[TEGRA_SE_AES_BLOCK_SIZE / 4];
	memcpy(data, ctr, TEGRA_SE_AES_BLOCK_SIZE);

	for (u32 i = 0; i < (TEGRA_SE_AES_BLOCK_SIZE / 4); i++)
		SE(SE_CRYPTO_CTR_REG_OFFSET + (4 * i)) = data[i];
}

void se_rsa_acc_ctrl(u32 rs, u32 flags)
{
	if (flags & SE_RSA_KEY_TBL_DIS_KEY_ALL_FLAG)
		SE(SE_RSA_KEYTABLE_ACCESS_REG_OFFSET + 4 * rs) =
			((flags >> SE_RSA_KEY_TBL_DIS_KEYUSE_FLAG_SHIFT) & SE_RSA_KEY_TBL_DIS_KEYUSE_FLAG) |
			((flags & SE_RSA_KEY_TBL_DIS_KEY_READ_UPDATE_FLAG) ^ SE_RSA_KEY_TBL_DIS_KEY_ALL_COMMON_FLAG);
	if (flags & SE_RSA_KEY_TBL_DIS_KEY_LOCK_FLAG)
		SE(SE_RSA_KEYTABLE_ACCESS_LOCK_OFFSET) &= ~BIT(rs);
}

// se_rsa_key_set() was derived from Atmosphère's set_rsa_keyslot
void se_rsa_key_set(u32 ks, const void *mod, u32 mod_size, const void *exp, u32 exp_size)
{
	u32 *data = (u32 *)mod;
	for (u32 i = 0; i < mod_size / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) | i;
		SE(SE_RSA_KEYTABLE_DATA) = byte_swap_32(data[mod_size / 4 - i - 1]);
	}

	data = (u32 *)exp;
	for (u32 i = 0; i < exp_size / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) | i;
		SE(SE_RSA_KEYTABLE_DATA) = byte_swap_32(data[exp_size / 4 - i - 1]);
	}

	_se_rsa_mod_sizes[ks] = mod_size;
	_se_rsa_exp_sizes[ks] = exp_size;
}

// se_rsa_key_clear() was derived from Atmosphère's clear_rsa_keyslot
void se_rsa_key_clear(u32 ks)
{
	for (u32 i = 0; i < TEGRA_SE_RSA2048_DIGEST_SIZE / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) | i;
		SE(SE_RSA_KEYTABLE_DATA) = 0;
	}
	for (u32 i = 0; i < TEGRA_SE_RSA2048_DIGEST_SIZE / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) | i;
		SE(SE_RSA_KEYTABLE_DATA) = 0;
	}
}

// se_rsa_exp_mod() was derived from Atmosphère's se_synchronous_exp_mod and se_get_exp_mod_output
int se_rsa_exp_mod(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	int res;
	u8 stack_buf[TEGRA_SE_RSA2048_DIGEST_SIZE];

	for (u32 i = 0; i < src_size; i++)
		stack_buf[i] = *((u8 *)src + src_size - i - 1);

	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_RSA) | SE_CONFIG_DST(DST_RSAREG);
	SE(SE_RSA_CONFIG) = RSA_KEY_SLOT(ks);
	SE(SE_RSA_KEY_SIZE_REG_OFFSET) = (_se_rsa_mod_sizes[ks] >> 6) - 1;
	SE(SE_RSA_EXP_SIZE_REG_OFFSET) = _se_rsa_exp_sizes[ks] >> 2;

	res = _se_execute_oneshot(OP_START, NULL, 0, stack_buf, src_size);

	// Copy output hash.
	u32 *dst32 = (u32 *)dst;
	for (u32 i = 0; i < dst_size / 4; i++)
		dst32[dst_size / 4 - i - 1] = byte_swap_32(SE(SE_RSA_OUTPUT + (i << 2)));

	return res;
}

void se_key_acc_ctrl(u32 ks, u32 flags)
{
	if (flags & SE_KEY_TBL_DIS_KEY_ACCESS_FLAG)
		SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 4 * ks) = ~flags;
	if (flags & SE_KEY_TBL_DIS_KEY_LOCK_FLAG)
		SE(SE_KEY_TABLE_ACCESS_LOCK_OFFSET) &= ~BIT(ks);
}

u32 se_key_acc_ctrl_get(u32 ks)
{
	return SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 4 * ks);
}

void se_aes_key_set(u32 ks, const void *key, u32 size)
{
	u32 data[TEGRA_SE_AES_MAX_KEY_SIZE / 4];
	memcpy(data, key, size);

	for (u32 i = 0; i < (size / 4); i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = data[i];
	}
}

void se_aes_key_partial_set(u32 ks, u32 index, u32 data)
{
	SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | index;
	SE(SE_KEYTABLE_DATA0_REG_OFFSET) = data;
}

void se_aes_iv_set(u32 ks, const void *iv)
{
	u32 data[TEGRA_SE_AES_BLOCK_SIZE / 4];
	memcpy(data, iv, TEGRA_SE_AES_BLOCK_SIZE);

	for (u32 i = 0; i < (TEGRA_SE_AES_BLOCK_SIZE / 4); i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | SE_KEYTABLE_QUAD(QUAD_ORG_IV) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = data[i];
	}
}

void se_aes_key_get(u32 ks, void *key, u32 size)
{
	u32 data[TEGRA_SE_AES_MAX_KEY_SIZE / 4];

	for (u32 i = 0; i < (size / 4); i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		data[i] = SE(SE_KEYTABLE_DATA0_REG_OFFSET);
	}

	memcpy(key, data, size);
}

void se_aes_key_clear(u32 ks)
{
	for (u32 i = 0; i < (TEGRA_SE_AES_MAX_KEY_SIZE / 4); i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = 0;
	}
}

void se_aes_iv_clear(u32 ks)
{
	for (u32 i = 0; i < (TEGRA_SE_AES_BLOCK_SIZE / 4); i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | SE_KEYTABLE_QUAD(QUAD_ORG_IV) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = 0;
	}
}

int se_aes_unwrap_key(u32 ks_dst, u32 ks_src, const void *input)
{
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_DEC_ALG(ALG_AES_DEC) | SE_CONFIG_DST(DST_KEYTAB);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks_src) | SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;
	SE(SE_CRYPTO_KEYTABLE_DST_REG_OFFSET) = SE_CRYPTO_KEYTABLE_DST_KEY_INDEX(ks_dst);

	return _se_execute_oneshot(OP_START, NULL, 0, input, 0x10);
}

int se_aes_crypt_ecb(u32 ks, u32 enc, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	if (enc)
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
	}
	else
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_DEC_ALG(ALG_AES_DEC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
	}
	SE(SE_BLOCK_COUNT_REG_OFFSET) = (src_size >> 4) - 1;
	return _se_execute_oneshot(OP_START, dst, dst_size, src, src_size);
}

int se_aes_crypt_cbc(u32 ks, u32 enc, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	if (enc)
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) | SE_CRYPTO_XOR_POS(XOR_TOP);
	}
	else
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_DEC_ALG(ALG_AES_DEC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_VCTRAM_SEL(VCTRAM_PREVAHB) |
			SE_CRYPTO_CORE_SEL(CORE_DECRYPT) | SE_CRYPTO_XOR_POS(XOR_BOTTOM);
	}
	SE(SE_BLOCK_COUNT_REG_OFFSET) = (src_size >> 4) - 1;
	return _se_execute_oneshot(OP_START, dst, dst_size, src, src_size);
}

int se_aes_crypt_block_ecb(u32 ks, u32 enc, void *dst, const void *src)
{
	return se_aes_crypt_ecb(ks, enc, dst, 0x10, src, 0x10);
}

int se_aes_crypt_ctr(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size, void *ctr)
{
	SE(SE_SPARE_0_REG_OFFSET) = 1;
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
		SE_CRYPTO_XOR_POS(XOR_BOTTOM) | SE_CRYPTO_INPUT_SEL(INPUT_LNR_CTR) | SE_CRYPTO_CTR_VAL(1);
	_se_aes_ctr_set(ctr);

	u32 src_size_aligned = src_size & 0xFFFFFFF0;
	u32 src_size_delta = src_size & 0xF;

	if (src_size_aligned)
	{
		SE(SE_BLOCK_COUNT_REG_OFFSET) = (src_size >> 4) - 1;
		if (!_se_execute_oneshot(OP_START, dst, dst_size, src, src_size_aligned))
			return 0;
	}

	if (src_size - src_size_aligned && src_size_aligned < dst_size)
		return _se_execute_one_block(OP_START, dst + src_size_aligned,
			MIN(src_size_delta, dst_size - src_size_aligned),
			src + src_size_aligned, src_size_delta);

	return 1;
}

// random calls were derived from Atmosphère's
int se_initialize_rng()
{
	static bool initialized = false;

	if (initialized)
		return 1;

	u8 *output_buf = (u8 *)malloc(0x10);

	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) | SE_CRYPTO_INPUT_SEL(INPUT_RANDOM);
	SE(SE_RNG_CONFIG_REG_OFFSET) = SE_RNG_CONFIG_MODE(RNG_MODE_FORCE_INSTANTION) | SE_RNG_CONFIG_SRC(RNG_SRC_ENTROPY);
	SE(SE_RNG_RESEED_INTERVAL_REG_OFFSET) = 70001;
	SE(SE_RNG_SRC_CONFIG_REG_OFFSET) = SE_RNG_SRC_CONFIG_ENT_SRC(RNG_SRC_RO_ENT_ENABLE) |
		SE_RNG_SRC_CONFIG_ENT_SRC_LOCK(RNG_SRC_RO_ENT_LOCK_ENABLE);
	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;

	int res =_se_execute_oneshot(OP_START, output_buf, 0x10, NULL, 0);

	free(output_buf);
	if (res)
		initialized = true;
	return res;
}

int se_generate_random(void *dst, u32 size)
{
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) | SE_CRYPTO_INPUT_SEL(INPUT_RANDOM);
	SE(SE_RNG_CONFIG_REG_OFFSET) = SE_RNG_CONFIG_MODE(RNG_MODE_NORMAL) | SE_RNG_CONFIG_SRC(RNG_SRC_ENTROPY);

	u32 num_blocks = size >> 4;
	u32 aligned_size = num_blocks << 4;
	if (num_blocks)
	{
		SE(SE_BLOCK_COUNT_REG_OFFSET) = num_blocks - 1;
		if (!_se_execute_oneshot(OP_START, dst, aligned_size, NULL, 0))
			return 0;
	}
	if (size > aligned_size)
		return _se_execute_one_block(OP_START, dst + aligned_size, size - aligned_size, NULL, 0);
	return 1;
}

int se_generate_random_key(u32 ks_dst, u32 ks_src)
{
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks_src) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
		SE_CRYPTO_INPUT_SEL(INPUT_RANDOM);
	SE(SE_RNG_CONFIG_REG_OFFSET) = SE_RNG_CONFIG_MODE(RNG_MODE_NORMAL) | SE_RNG_CONFIG_SRC(RNG_SRC_ENTROPY);

	SE(SE_CRYPTO_KEYTABLE_DST_REG_OFFSET) = SE_CRYPTO_KEYTABLE_DST_KEY_INDEX(ks_dst);
	if (!_se_execute_oneshot(OP_START, NULL, 0, NULL, 0))
		return 0;
	SE(SE_CRYPTO_KEYTABLE_DST_REG_OFFSET) = SE_CRYPTO_KEYTABLE_DST_KEY_INDEX(ks_dst) | 1;
	if (!_se_execute_oneshot(OP_START, NULL, 0, NULL, 0))
		return 0;

	return 1;
}

int se_aes_xts_crypt_sec(u32 tweak_ks, u32 crypt_ks, u32 enc, u64 sec, void *dst, const void *src, u32 sec_size)
{
	u8 tweak[0x10];
	u8 orig_tweak[0x10];
	u32 *pdst = (u32 *)dst;
	u32 *psrc = (u32 *)src;
	u32 *ptweak = (u32 *)tweak;

	//Generate tweak.
	for (int i = 0xF; i >= 0; i--)
	{
		tweak[i] = sec & 0xFF;
		sec >>= 8;
	}
	if (!se_aes_crypt_block_ecb(tweak_ks, 1, tweak, tweak))
		return 0;

	memcpy(orig_tweak, tweak, 0x10);

	// We are assuming a 0x10-aligned sector size in this implementation.
	for (u32 i = 0; i < sec_size / 0x10; i++)
	{
		for (u32 j = 0; j < 4; j++)
			pdst[j] = psrc[j] ^ ptweak[j];

		_gf256_mul_x_le(tweak);
		psrc += 4;
		pdst += 4;
	}

	if (!se_aes_crypt_ecb(crypt_ks, enc, dst, sec_size, dst, sec_size))
		return 0;

	pdst = (u32 *)dst;
	ptweak = (u32 *)orig_tweak;
	for (u32 i = 0; i < sec_size / 0x10; i++)
	{
		for (u32 j = 0; j < 4; j++)
			pdst[j] = pdst[j] ^ ptweak[j];

		_gf256_mul_x_le(orig_tweak);
		pdst += 4;
	}

	return 1;
}

int se_aes_xts_crypt(u32 tweak_ks, u32 crypt_ks, u32 enc, u64 sec, void *dst, const void *src, u32 sec_size, u32 num_secs)
{
	u8 *pdst = (u8 *)dst;
	u8 *psrc = (u8 *)src;

	for (u32 i = 0; i < num_secs; i++)
		if (!se_aes_xts_crypt_sec(tweak_ks, crypt_ks, enc, sec + i, pdst + sec_size * i, psrc + sec_size * i, sec_size))
			return 0;

	return 1;
}

// se_aes_cmac() was derived from Atmosphère's se_compute_aes_cmac
int se_aes_cmac(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	int res = 0;
	u8 *key = (u8 *)calloc(0x10, 1);
	u8 *last_block = (u8 *)calloc(0x10, 1);

	// generate derived key
	if (!se_aes_crypt_block_ecb(ks, 1, key, key))
		goto out;
	_gf256_mul_x(key);
	if (src_size & 0xF)
		_gf256_mul_x(key);

	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_HASHREG);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_INPUT_SEL(INPUT_AHB) |
		SE_CRYPTO_XOR_POS(XOR_TOP) | SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) | SE_CRYPTO_HASH(HASH_ENABLE) |
		SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
	se_aes_iv_clear(ks);

	u32 num_blocks = (src_size + 0xf) >> 4;
	if (num_blocks > 1)
	{
		SE(SE_BLOCK_COUNT_REG_OFFSET) = num_blocks - 2;
		if (!_se_execute_oneshot(OP_START, NULL, 0, src, src_size))
			goto out;
		SE(SE_CRYPTO_REG_OFFSET) |= SE_CRYPTO_IV_SEL(IV_UPDATED);
	}

	if (src_size & 0xf)
	{
		memcpy(last_block, src + (src_size & ~0xf), src_size & 0xf);
		last_block[src_size & 0xf] = 0x80;
	}
	else if (src_size >= 0x10)
	{
		memcpy(last_block, src + src_size - 0x10, 0x10);
	}

	for (u32 i = 0; i < 0x10; i++)
		last_block[i] ^= key[i];

	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;
	res = _se_execute_oneshot(OP_START, NULL, 0, last_block, 0x10);

	u32 *dst32 = (u32 *)dst;
	for (u32 i = 0; i < (dst_size >> 2); i++)
		dst32[i] = SE(SE_HASH_RESULT_REG_OFFSET + (i << 2));

out:;
	free(key);
	free(last_block);
	return res;
}

int se_calc_sha256(void *hash, u32 *msg_left, const void *src, u32 src_size, u64 total_size, u32 sha_cfg, bool is_oneshot)
{
	int res;
	u32 hash32[TEGRA_SE_SHA_256_SIZE / 4];

	//! TODO: src_size must be 512 bit aligned if continuing and not last block for SHA256.
	if (src_size > 0xFFFFFF || !hash) // Max 16MB - 1 chunks and aligned x4 hash buffer.
		return 0;

	// Setup config for SHA256.
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_MODE(MODE_SHA256) | SE_CONFIG_ENC_ALG(ALG_SHA) | SE_CONFIG_DST(DST_HASHREG);
	SE(SE_SHA_CONFIG_REG_OFFSET) = sha_cfg;
	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;

	// Set total size to current buffer size if empty.
	if (!total_size)
		total_size = src_size;

	// Set total size: BITS(src_size), up to 2 EB.
	SE(SE_SHA_MSG_LENGTH_0_REG_OFFSET) = (u32)(total_size << 3);
	SE(SE_SHA_MSG_LENGTH_1_REG_OFFSET) = (u32)(total_size >> 29);
	SE(SE_SHA_MSG_LENGTH_2_REG_OFFSET) = 0;
	SE(SE_SHA_MSG_LENGTH_3_REG_OFFSET) = 0;

	// Set size left to hash.
	SE(SE_SHA_MSG_LEFT_0_REG_OFFSET) = (u32)(total_size << 3);
	SE(SE_SHA_MSG_LEFT_1_REG_OFFSET) = (u32)(total_size >> 29);
	SE(SE_SHA_MSG_LEFT_2_REG_OFFSET) = 0;
	SE(SE_SHA_MSG_LEFT_3_REG_OFFSET) = 0;

	// If we hash in chunks, copy over the intermediate.
	if (sha_cfg == SHA_CONTINUE && msg_left)
	{
		// Restore message left to process.
		SE(SE_SHA_MSG_LEFT_0_REG_OFFSET) = msg_left[0];
		SE(SE_SHA_MSG_LEFT_1_REG_OFFSET) = msg_left[1];

		// Restore hash reg.
		memcpy(hash32, hash, TEGRA_SE_SHA_256_SIZE);
		for (u32 i = 0; i < (TEGRA_SE_SHA_256_SIZE / 4); i++)
			SE(SE_HASH_RESULT_REG_OFFSET + (i << 2)) = byte_swap_32(hash32[i]);
	}

	// Trigger the operation.
	res = _se_execute(OP_START, NULL, 0, src, src_size, is_oneshot);

	if (is_oneshot)
	{
		// Backup message left.
		if (msg_left)
		{
			msg_left[0] = SE(SE_SHA_MSG_LEFT_0_REG_OFFSET);
			msg_left[1] = SE(SE_SHA_MSG_LEFT_1_REG_OFFSET);
		}

		// Copy output hash.
		for (u32 i = 0; i < (TEGRA_SE_SHA_256_SIZE / 4); i++)
			hash32[i] = byte_swap_32(SE(SE_HASH_RESULT_REG_OFFSET + (i << 2)));
		memcpy(hash, hash32, TEGRA_SE_SHA_256_SIZE);
	}

	return res;
}

int se_calc_sha256_oneshot(void *hash, const void *src, u32 src_size)
{
	return se_calc_sha256(hash, NULL, src, src_size, 0, SHA_INIT_HASH, true);
}

int se_calc_sha256_finalize(void *hash, u32 *msg_left)
{
	u32 hash32[TEGRA_SE_SHA_256_SIZE / 4];
	int res = _se_execute_finalize();

	// Backup message left.
	if (msg_left)
	{
		msg_left[0] = SE(SE_SHA_MSG_LEFT_0_REG_OFFSET);
		msg_left[1] = SE(SE_SHA_MSG_LEFT_1_REG_OFFSET);
	}

	// Copy output hash.
	for (u32 i = 0; i < (TEGRA_SE_SHA_256_SIZE / 4); i++)
		hash32[i] = byte_swap_32(SE(SE_HASH_RESULT_REG_OFFSET + (i << 2)));
	memcpy(hash, hash32, TEGRA_SE_SHA_256_SIZE);

	return res;
}

int se_calc_hmac_sha256(void *dst, const void *src, u32 src_size, const void *key, u32 key_size)
{
	int res = 0;
	u8 *secret = (u8 *)malloc(0x40);
	u8 *ipad = (u8 *)malloc(0x40 + src_size);
	u8 *opad = (u8 *)malloc(0x60);

	if (key_size > 0x40)
	{
		if (!se_calc_sha256_oneshot(secret, key, key_size))
			goto out;
		memset(secret + 0x20, 0, 0x20);
	}
	else
	{
		memcpy(secret, key, key_size);
		memset(secret + key_size, 0, 0x40 - key_size);
	}

	u32 *secret32 = (u32 *)secret;
	u32 *ipad32 = (u32 *)ipad;
	u32 *opad32 = (u32 *)opad;
	for (u32 i = 0; i < 0x10; i++)
	{
		ipad32[i] = secret32[i] ^ 0x36363636;
		opad32[i] = secret32[i] ^ 0x5C5C5C5C;
	}

	memcpy(ipad + 0x40, src, src_size);
	if (!se_calc_sha256_oneshot(dst, ipad, 0x40 + src_size))
		goto out;
	memcpy(opad + 0x40, dst, 0x20);
	if (!se_calc_sha256_oneshot(dst, opad, 0x60))
		goto out;

	res = 1;

out:;
	free(secret);
	free(ipad);
	free(opad);
	return res;
}

// _mgf1_xor() and rsa_oaep_decode were derived from Atmosphère
static void _mgf1_xor(void *masked, u32 masked_size, const void *seed, u32 seed_size)
{
    u8 cur_hash[0x20] __attribute__((aligned(4)));
    u8 hash_buf[0xe4] __attribute__((aligned(4)));

    u32 hash_buf_size = seed_size + 4;
    memcpy(hash_buf, seed, seed_size);
    u32 round_num = 0;

    u8 *p_out = (u8 *)masked;

    while (masked_size) {
        u32 cur_size = MIN(masked_size, 0x20);

        for (u32 i = 0; i < 4; i++)
            hash_buf[seed_size + 3 - i] = (round_num >> (8 * i)) & 0xff;
        round_num++;

        se_calc_sha256_oneshot(cur_hash, hash_buf, hash_buf_size);

        for (unsigned int i = 0; i < cur_size; i++) {
            *p_out ^= cur_hash[i];
            p_out++;
        }

        masked_size -= cur_size;
    }
}

u32 se_rsa_oaep_decode(void *dst, u32 dst_size, const void *label_digest, u32 label_digest_size, u8 *buf, u32 buf_size)
{
	if (dst_size <= 0 || buf_size < 0x43 || label_digest_size != 0x20)
		return 0;

	bool is_valid = buf[0] == 0;

	u32 db_len = buf_size - 0x21;
	u8 *seed = buf + 1;
	u8 *db = seed + 0x20;
	_mgf1_xor(seed, 0x20, db, db_len);
	_mgf1_xor(db, db_len, seed, 0x20);

	is_valid &= memcmp(label_digest, db, 0x20) ? 0 : 1;

	db += 0x20;
	db_len -= 0x20;

	int msg_ofs = 0;
	int looking_for_one = 1;
	int invalid_db_padding = 0;
	int is_zero;
	int is_one;
	for (int i = 0; i < db_len; )
	{
		is_zero = (db[i] == 0);
		is_one  = (db[i] == 1);
		msg_ofs += (looking_for_one & is_one) * (++i);
		looking_for_one &= ~is_one;
		invalid_db_padding |= (looking_for_one & ~is_zero);
	}

	is_valid &= (invalid_db_padding == 0);

	const u32 msg_size = MIN(dst_size, is_valid * (db_len - msg_ofs));
	memcpy(dst, db + msg_ofs, msg_size);

	return msg_size;
}

void se_get_aes_keys(u8 *buf, u8 *keys, u32 keysize)
{
	u8 *aligned_buf = (u8 *)ALIGN((u32)buf, 0x40);

	// Set Secure Random Key.
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_MODE(MODE_KEY128) | SE_CONFIG_ENC_ALG(ALG_RNG) | SE_CONFIG_DST(DST_SRK);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(0) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) | SE_CRYPTO_INPUT_SEL(INPUT_RANDOM);
	SE(SE_RNG_CONFIG_REG_OFFSET) = SE_RNG_CONFIG_SRC(RNG_SRC_ENTROPY) | SE_RNG_CONFIG_MODE(RNG_MODE_FORCE_RESEED);
	SE(SE_CRYPTO_LAST_BLOCK) = 0;
	_se_execute_oneshot(OP_START, NULL, 0, NULL, 0);

	// Save AES keys.
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_MODE(MODE_KEY128) | SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);

	for (u32 i = 0; i < TEGRA_SE_KEYSLOT_COUNT; i++)
	{
		SE(SE_CONTEXT_SAVE_CONFIG_REG_OFFSET) = SE_CONTEXT_SAVE_SRC(AES_KEYTABLE) |
			(i << SE_KEY_INDEX_SHIFT) | SE_CONTEXT_SAVE_WORD_QUAD(KEYS_0_3);

		SE(SE_CRYPTO_LAST_BLOCK) = 0;
		_se_execute_oneshot(OP_CTX_SAVE, aligned_buf, 0x10, NULL, 0);
		memcpy(keys + i * keysize, aligned_buf, 0x10);

		if (keysize > 0x10)
		{
			SE(SE_CONTEXT_SAVE_CONFIG_REG_OFFSET) = SE_CONTEXT_SAVE_SRC(AES_KEYTABLE) |
				(i << SE_KEY_INDEX_SHIFT) | SE_CONTEXT_SAVE_WORD_QUAD(KEYS_4_7);

			SE(SE_CRYPTO_LAST_BLOCK) = 0;
			_se_execute_oneshot(OP_CTX_SAVE, aligned_buf, 0x10, NULL, 0);
			memcpy(keys + i * keysize + 0x10, aligned_buf, 0x10);
		}
	}

	// Save SRK to PMC secure scratches.
	SE(SE_CONTEXT_SAVE_CONFIG_REG_OFFSET) = SE_CONTEXT_SAVE_SRC(SRK);
	SE(SE_CRYPTO_LAST_BLOCK) = 0;
	_se_execute_oneshot(OP_CTX_SAVE, NULL, 0, NULL, 0);

	// End context save.
	SE(SE_CONFIG_REG_OFFSET) = 0;
	_se_execute_oneshot(OP_CTX_SAVE, NULL, 0, NULL, 0);

	// Get SRK.
	u32 srk[4];
	srk[0] = PMC(APBDEV_PMC_SECURE_SCRATCH4);
	srk[1] = PMC(APBDEV_PMC_SECURE_SCRATCH5);
	srk[2] = PMC(APBDEV_PMC_SECURE_SCRATCH6);
	srk[3] = PMC(APBDEV_PMC_SECURE_SCRATCH7);

	// Decrypt context.
	se_aes_key_clear(3);
	se_aes_key_set(3, srk, 0x10);
	se_aes_crypt_cbc(3, 0, keys, TEGRA_SE_KEYSLOT_COUNT * keysize, keys, TEGRA_SE_KEYSLOT_COUNT * keysize);
	se_aes_key_clear(3);
}
