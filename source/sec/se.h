/*
* Copyright (c) 2018 naehrwert
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

#ifndef _SE_H_
#define _SE_H_

#include "../utils/types.h"

void se_rsa_acc_ctrl(u32 rs, u32 flags);
void se_rsa_key_set(u32 ks, const void *mod, u32 mod_size, const void *exp, u32 exp_size);
void se_rsa_key_clear(u32 ks);
int se_rsa_exp_mod(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size);
void se_key_acc_ctrl(u32 ks, u32 flags);
void se_aes_key_set(u32 ks, const void *key, u32 size);
void se_aes_key_read(u32 ks, void *key, u32 size);
void se_aes_key_clear(u32 ks);
int se_aes_unwrap_key(u32 ks_dst, u32 ks_src, const void *input);
int se_aes_crypt_ecb(u32 ks, u32 enc, void *dst, u32 dst_size, const void *src, u32 src_size);
int se_aes_crypt_block_ecb(u32 ks, u32 enc, void *dst, const void *src);
int se_aes_crypt_ctr(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size, void *ctr);
int se_aes_xts_crypt_sec(u32 ks1, u32 ks2, u32 enc, u64 sec, void *dst, const void *src, u32 secsize);
int se_aes_xts_crypt(u32 ks1, u32 ks2, u32 enc, u64 sec, void *dst, const void *src, u32 secsize, u32 num_secs);
int se_aes_cmac(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size);
int se_calc_sha256(void *dst, const void *src, u32 src_size);
int se_calc_hmac_sha256(void *dst, const void *src, u32 src_size, const void *key, u32 key_size);

#endif
