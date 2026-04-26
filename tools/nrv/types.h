#pragma once

typedef signed char s8;
typedef short s16;
typedef int s32;
typedef long long int s64;

// typedef int bool;
typedef unsigned char byte;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;


#define MAX(a,b)        ((a) >= (b) ? (a) : (b))
#define MIN(a,b)        ((a) <= (b) ? (a) : (b))

struct compress_config_t
{
    int bb_endian;
    int bb_size;
    u32 max_offset;
    u32 max_match;
    int s_level;
    int h_level;
    int p_level;
    int c_flags;
    u32 m_size;
};

typedef struct
{
    int init;

    u32 look;          /* bytes in lookahead buffer */

    u32 m_len;
    u32 m_off;

    u32 last_m_len;
    u32 last_m_off;

    const byte *bp;
    const byte *ip;
    const byte *in;
    const byte *in_end;
    byte *out;

    u32 bb_b;
    unsigned bb_k;
    unsigned bb_c_endian;
    unsigned bb_c_s;
    unsigned bb_c_s8;
    byte *bb_p;
    byte *bb_op;

    struct compress_config_t conf;
    u32 *result;

    // ucl_progress_callback_p cb;

    u32 textsize;      /* text size counter */
    u32 codesize;      /* code size counter */
    u32 printcount;    /* counter for reporting progress every 1K bytes */

    /* some stats */
    unsigned long lit_bytes;
    unsigned long match_bytes;
    unsigned long rep_bytes;
    unsigned long lazy;
} COMPRESS_T;

#define N       (1024*1024ul)       /* size of ring buffer */
#define SWD_USE_MALLOC
#define SWD_HSIZE   65536ul
#define THRESHOLD         1           /* lower limit for match length */
#define F              2048           /* upper limit for match length */
#define HSIZE         16384
#define MAX_CHAIN      2048
#define M2_MAX_OFFSET 0x500

typedef struct
{
/* public - "built-in" */
    u32 n;
    u32 f;
    u32 threshold;

/* public - configuration */
    u32 max_chain;
    u32 nice_length;
    bool use_best_off;
    u32 lazy_insert;

/* public - output */
    u32 m_len;
    u32 m_off;
    u32 look;
    int b_char;
#if defined(SWD_BEST_OFF)
    u32 best_off[ SWD_BEST_OFF ];
#endif

/* semi public */
    COMPRESS_T *c;
    u32 m_pos;
#if defined(SWD_BEST_OFF)
    u32 best_pos[ SWD_BEST_OFF ];
#endif

/* private */
    const byte *dict;
    const byte *dict_end;
    u32 dict_len;

/* private */
    u32 ip;                /* input pointer (lookahead) */
    u32 bp;                /* buffer pointer */
    u32 rp;                /* remove pointer */
    u32 b_size;

    unsigned char *b_wrap;

    u32 node_count;
    u32 first_rp;

#if defined(SWD_USE_MALLOC)
    unsigned char *b;
    u32 *head3;
    u32 *succ3;
    u32 *best3;
    u32 *llen3;
#ifdef HEAD2
    u32 *head2;
#endif
#else
    unsigned char b [ N + F + F ];
    u32 head3 [ HSIZE ];
    u32 succ3 [ N + F ];
    u32 best3 [ N + F ];
    u32 llen3 [ HSIZE ];
#ifdef HEAD2
    u32 head2 [ UCL_UINT32_C(65536) ];
#endif
#endif
} swd_t;
