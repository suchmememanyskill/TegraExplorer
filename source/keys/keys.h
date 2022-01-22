#pragma once
#include <utils/types.h>

#define HOS_PKG11_MAGIC 0x31314B50
#define HOS_EKS_MAGIC   0x30534B45

#define AES_128_KEY_SIZE 16
#define RSA_2048_KEY_SIZE 256

// only tickets of type Rsa2048Sha256 are expected
typedef struct {
    u32 signature_type;   // always 0x10004
    u8 signature[RSA_2048_KEY_SIZE];
    u8 sig_padding[0x3C];
    char issuer[0x40];
    u8 titlekey_block[RSA_2048_KEY_SIZE];
    u8 format_version;
    u8 titlekey_type;
    u16 ticket_version;
    u8 license_type;
    u8 common_key_id;
    u16 property_mask;
    u64 reserved;
    u64 ticket_id;
    u64 device_id;
    u8 rights_id[0x10];
    u32 account_id;
    u32 sect_total_size;
    u32 sect_hdr_offset;
    u16 sect_hdr_count;
    u16 sect_hdr_entry_size;
    u8 padding[0x140];
} ticket_t;

typedef struct {
    u8 rights_id[0x10];
    u64 ticket_id;
    u32 account_id;
    u16 property_mask;
    u16 reserved;
} ticket_record_t;

typedef struct {
    u8 read_buffer[0x40000];
    u8 rights_ids[0x40000 / 0x10][0x10];
    u8 titlekeys[0x40000 / 0x10][0x10];
} titlekey_buffer_t;

typedef struct {
    u8 private_exponent[RSA_2048_KEY_SIZE];
    u8 modulus[RSA_2048_KEY_SIZE];
    u8 public_exponent[4];
    u8 reserved[0x14];
    u64 device_id;
    u8 gmac[0x10];
} rsa_keypair_t;

typedef struct {
    u8 master_kek[AES_128_KEY_SIZE];
    u8 data[0x70];
    u8 package1_key[AES_128_KEY_SIZE];
} keyblob_t;

typedef struct {
    u8 cmac[0x10];
    u8 iv[0x10];
    keyblob_t key_data;
    u8 unused[0x150];
} encrypted_keyblob_t;

typedef struct {
    u8  temp_key[AES_128_KEY_SIZE],
        bis_key[3][AES_128_KEY_SIZE * 2],
        device_key[AES_128_KEY_SIZE],
        device_key_4x[AES_128_KEY_SIZE],
        // FS-related keys
        header_key[AES_128_KEY_SIZE * 2],
        save_mac_key[AES_128_KEY_SIZE],
        // keyblob-derived families
        keyblob_key[AES_128_KEY_SIZE],
        keyblob_mac_key[AES_128_KEY_SIZE],
        package1_key[AES_128_KEY_SIZE],
        // master key-derived families,
        master_kek[AES_128_KEY_SIZE],
        master_key[AES_128_KEY_SIZE],
        tsec_keys[AES_128_KEY_SIZE * 2];
    u32 sbk[4];
    keyblob_t keyblob;
} key_derivation_ctx_t;

typedef struct _tsec_key_data_t
{
	u8 debug_key[0x10];
	u8 blob0_auth_hash[0x10];
	u8 blob1_auth_hash[0x10];
	u8 blob2_auth_hash[0x10];
	u8 blob2_aes_iv[0x10];
	u8 hovi_eks_seed[0x10];
	u8 hovi_common_seed[0x10];
	u32 blob0_size;
	u32 blob1_size;
	u32 blob2_size;
	u32 blob3_size;
	u32 blob4_size;
	u8 reserved[0x7C];
} tsec_key_data_t;

int DumpKeys();
void PrintKey(u8 *key, u32 len);

extern key_derivation_ctx_t __attribute__((aligned(4))) dumpedKeys;