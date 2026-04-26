#include "swd.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>   

static void bbWriteBits(COMPRESS_T *c)
{
    byte *p = c->bb_p;
    u32 b = c->bb_b;

    p[0] = (b >>  0);
    if (c->bb_c_s >= 16)
    {
        p[1] = (b >>  8);
        if (c->bb_c_s == 32)
        {
            p[2] = (b >> 16);
            p[3] = (b >> 24);
        }
    }
}

static void bbPutByte(COMPRESS_T *c, unsigned b)
{
    /**printf("putbyte %p %p %x  (%d)\n", op, bb_p, x, bb_k);*/
    assert(c->bb_p == NULL || c->bb_p + c->bb_c_s8 <= c->bb_op);
    *c->bb_op++ = (b);
}


static void bbPutBit(COMPRESS_T *c, unsigned bit)
{
    assert(bit == 0 || bit == 1);
    assert(c->bb_k <= c->bb_c_s);

    if (c->bb_k < c->bb_c_s)
    {
        if (c->bb_k == 0)
        {
            assert(c->bb_p == NULL);
            c->bb_p = c->bb_op;
            c->bb_op += c->bb_c_s8;
        }
        assert(c->bb_p != NULL);
        assert(c->bb_p + c->bb_c_s8 <= c->bb_op);

        c->bb_b = (c->bb_b << 1) + bit;
        c->bb_k++;
    }
    else
    {
        assert(c->bb_p != NULL);
        assert(c->bb_p + c->bb_c_s8 <= c->bb_op);

        bbWriteBits(c);
        c->bb_p = c->bb_op;
        c->bb_op += c->bb_c_s8;
        c->bb_b = bit;
        c->bb_k = 1;
    }
}

static void bbFlushBits(COMPRESS_T *c, unsigned filler_bit)
{
    if (c->bb_k > 0)
    {
        assert(c->bb_k <= c->bb_c_s);
        while (c->bb_k != c->bb_c_s)
            bbPutBit(c, filler_bit);
        bbWriteBits(c);
        c->bb_k = 0;
    }
    c->bb_p = NULL;
}

static void code_prefix_ss11(COMPRESS_T *c, u32 i)
{
    if (i >= 2)
    {
        u32 t = 4;
        i += 2;
        do {
            t <<= 1;
        } while (i >= t);
        t >>= 1;
        do {
            t >>= 1;
            bbPutBit(c, (i & t) ? 1 : 0);
            bbPutBit(c, 0);
        } while (t > 2);
    }
    bbPutBit(c, (unsigned)i & 1);
    bbPutBit(c, 1);
}


static void code_prefix_ss12(COMPRESS_T *c, u32 i)
{
    if (i >= 2)
    {
        u32 t = 2;
        do {
            i -= t;
            t <<= 2;
        } while (i >= t);
        do {
            t >>= 1;
            bbPutBit(c, (i & t) ? 1 : 0);
            bbPutBit(c, 0);
            t >>= 1;
            bbPutBit(c, (i & t) ? 1 : 0);
        } while (t > 2);
    }
    bbPutBit(c, (unsigned)i & 1);
    bbPutBit(c, 1);
}

static void code_match(COMPRESS_T *c, u32 m_len, const u32 m_off)
{
    unsigned m_low = 0;

    while (m_len > c->conf.max_match)
    {
        code_match(c, c->conf.max_match - 3, m_off);
        m_len -= c->conf.max_match - 3;
    }

    c->match_bytes += m_len;
    if (m_len > c->result[3])
        c->result[3] = m_len;
    if (m_off > c->result[1])
        c->result[1] = m_off;

    bbPutBit(c, 0);

    m_len = m_len - 1 - (m_off > M2_MAX_OFFSET);
    assert(m_len > 0);
    m_low = (m_len <= 2);
    if (m_off == c->last_m_off)
    {
        bbPutBit(c, 0);
        bbPutBit(c, 1);
        bbPutBit(c, m_low);
    }
    else
    {
        code_prefix_ss12(c, 1 + ((m_off - 1) >> 7));
        bbPutByte(c, ((((unsigned)m_off - 1) & 0x7f) << 1) | (m_low ^ 1));
    }
    if (m_low)
        bbPutBit(c, (unsigned)m_len - 1);
    else if (m_len <= 4)
    {
        bbPutBit(c, 1);
        bbPutBit(c, (unsigned)m_len - 3);
    }
    else
    {
        bbPutBit(c, 0);
        code_prefix_ss11(c, m_len - 5);
    }

    c->last_m_off = m_off;
    // UCL_UNUSED(m_low);
}

static void code_run(COMPRESS_T *c, const byte *ii, u32 lit)
{
    if (lit == 0)
        return;
    c->lit_bytes += lit;
    if (lit > c->result[5])
        c->result[5] = lit;
    do {
        bbPutBit(c, 1);
        bbPutByte(c, *ii++);
    } while (--lit > 0);
}

static void assert_match( const swd_t *swd, u32 m_len, u32 m_off )
{
    const COMPRESS_T *c = swd->c;
    u32 d_off;

    assert(m_len >= 2);
    if (m_off <= (u32) (c->bp - c->in))
    {
        assert(c->bp - m_off + m_len < c->ip);
        assert(memcmp(c->bp, c->bp - m_off, m_len) == 0);
    }
    else
    {
        assert(swd->dict != NULL);
        d_off = m_off - (u32) (c->bp - c->in);
        assert(d_off <= swd->dict_len);
        if (m_len > d_off)
        {
            assert(memcmp(c->bp, swd->dict_end - d_off, d_off) == 0);
            assert(c->in + m_len - d_off < c->ip);
            assert(memcmp(c->bp + d_off, c->in, m_len - d_off) == 0);
        }
        else
        {
            assert(memcmp(c->bp, swd->dict_end - d_off, m_len) == 0);
        }
    }
}

static int find_match ( COMPRESS_T *c, swd_t *s,
             u32 this_len, u32 skip )
{
    assert(c->init);

    if (skip > 0)
    {
        assert(this_len >= skip);
        swd_accept(s, this_len - skip);
        c->textsize += this_len - skip + 1;
    }
    else
    {
        assert(this_len <= 1);
        c->textsize += this_len - skip;
    }

    s->m_len = THRESHOLD;
#ifdef SWD_BEST_OFF
    if (s->use_best_off)
        memset(s->best_pos,0,sizeof(s->best_pos));
#endif
    swd_findbest(s);
    c->m_len = s->m_len;
#if defined(__UCL_CHECKER)
    /* s->m_off may be uninitialized if we didn't find a match,
     * but then its value will never be used.
     */
    c->m_off = (s->m_len == THRESHOLD) ? 0 : s->m_off;
#else
    c->m_off = s->m_off;
#endif

    swd_getbyte(s);

    if (s->b_char < 0)
    {
        c->look = 0;
        c->m_len = 0;
        swd_exit(s);
    }
    else
    {
        c->look = s->look + 1;
    }
    c->bp = c->ip - c->look;

#if 0
    /* brute force match search */
    if (c->m_len > THRESHOLD && c->m_len + 1 <= c->look)
    {
        const ucl_byte *ip = c->bp;
        const ucl_byte *m  = c->bp - c->m_off;
        const ucl_byte *in = c->in;

        if (ip - in > N)
            in = ip - N;
        for (;;)
        {
            while (*in != *ip)
                in++;
            if (in == ip)
                break;
            if (in != m)
                if (memcmp(in,ip,c->m_len+1) == 0)
                    printf("%p %p %p %5d\n",in,ip,m,c->m_len);
            in++;
        }
    }
#endif

    // if (c->cb && c->textsize > c->printcount)
    // {
        // (*c->cb->callback)(c->textsize,c->codesize,3,c->cb->user);
        // c->printcount += 1024;
    // }

    return 0;
}

static int len_of_coded_match(COMPRESS_T *c, u32 m_len, u32 m_off)
{
    int b;
    if (m_len < 2 || (m_len == 2 && (m_off > M2_MAX_OFFSET))
        || m_off > c->conf.max_offset)
        return -1;
    assert(m_off > 0);

    m_len = m_len - 2 - (m_off > M2_MAX_OFFSET);

    if (m_off == c->last_m_off)
        b = 1 + 2;
    else
    {
        b = 1 + 9;
        m_off = (m_off - 1) >> 7;
        while (m_off > 0)
        {
            b += 3;
            m_off >>= 2;
        }
    }

    b += 2;
    if (m_len < 2)
        return b;
    if (m_len < 4)
        return b + 1;
    m_len -= 4;
#
    do {
        b += 2;
        m_len >>= 1;
    } while (m_len > 0);

    return b;
}

static int bbConfig(COMPRESS_T *c, int endian, int bitsize)
{
    if (endian != -1)
    {
        if (endian != 0)
            return -1;
        c->bb_c_endian = endian;
    }
    if (bitsize != -1)
    {
        if (bitsize != 8 && bitsize != 16 && bitsize != 32)
            return -1;
        c->bb_c_s = bitsize;
        c->bb_c_s8 = bitsize / 8;
    }
    c->bb_b = 0; c->bb_k = 0;
    c->bb_p = NULL;
    c->bb_op = NULL;
    return 0;
}


static int init_match ( COMPRESS_T *c, swd_t *s,
             const byte *dict, u32 dict_len,
             u32 flags )
{
    int r;

    assert(!c->init);
    c->init = 1;

    s->c = c;

    c->last_m_len = c->last_m_off = 0;

    c->textsize = c->codesize = c->printcount = 0;
    c->lit_bytes = c->match_bytes = c->rep_bytes = 0;
    c->lazy = 0;

    r = swd_init(s,dict,dict_len);
    if (r != 0)
    {
        swd_exit(s);
        return r;
    }

    s->use_best_off = (flags & 1) ? 1 : 0;
    return 0;
}

int nrv2e_99_compress( const byte * in, u32 in_len,
                         byte * out, u32 * out_len,
                         // ucl_progress_callback_p cb,
                         int level,
                         const struct compress_config_t *conf,
                         u32 * result)
{
    const byte *ii;
    u32 lit;
    u32 m_len, m_off;
    COMPRESS_T c_buffer;
    COMPRESS_T * const c = &c_buffer;
#undef swd
#if 1 && defined(SWD_USE_MALLOC)
    swd_t the_swd;
#   define swd (&the_swd)
#else
    swd_t *swd;
#endif
    u32 result_buffer[16];
    int r;

    struct swd_config_t
    {
        unsigned try_lazy;
        u32 good_length;
        u32 max_lazy;
        u32 nice_length;
        u32 max_chain;
        u32 flags;
        u32 max_offset;
    };
    const struct swd_config_t *sc;
    static const struct swd_config_t swd_config[10] = {
        /* faster compression */
        {   0,   0,   0,   8,    4,   0,  48*1024L },
        {   0,   0,   0,  16,    8,   0,  48*1024L },
        {   0,   0,   0,  32,   16,   0,  48*1024L },
        {   1,   4,   4,  16,   16,   0,  48*1024L },
        {   1,   8,  16,  32,   32,   0,  48*1024L },
        {   1,   8,  16, 128,  128,   0,  48*1024L },
        {   2,   8,  32, 128,  256,   0, 128*1024L },
        {   2,  32, 128,   F, 2048,   1, 128*1024L },
        {   2,  32, 128,   F, 2048,   1, 256*1024L },
        {   2,   F,   F,   F, 4096,   1, N }
        /* max. compression */
    };

    if (level < 1 || level > 10)
        return -2;
    sc = &swd_config[level - 1];

    memset(c, 0, sizeof(*c));
    c->ip = c->in = in;
    c->in_end = in + in_len;
    c->out = out;
    // if (cb && cb->callback)
        // c->cb = cb;
    // cb = NULL;
    c->result = result ? result : (u32 *) result_buffer;
    memset(c->result, 0, 16*sizeof(*c->result));
    c->result[0] = c->result[2] = c->result[4];
    result = NULL;
    memset(&c->conf, 0xff, sizeof(c->conf));
    if (conf)
        memcpy(&c->conf, conf, sizeof(c->conf));
    conf = NULL;
    r = bbConfig(c, 0, 8);
    if (r == 0)
        r = bbConfig(c, c->conf.bb_endian, c->conf.bb_size);
    if (r != 0)
        return -2;
    c->bb_op = out;

    ii = c->ip;             /* point to start of literal run */
    lit = 0;

#if !defined(swd)
    swd = (swd_t *) malloc(sizeof(*swd));
    if (!swd)
        return -3;
#endif
    swd->f = MIN(F, c->conf.max_match);
    swd->n = MIN(N, sc->max_offset);
    if (c->conf.max_offset != 32)
        swd->n = MIN(N, c->conf.max_offset);
    if (in_len >= 256 && in_len < swd->n)
        swd->n = in_len;
    if (swd->f < 8 || swd->n < 256)
        return -2;
    r = init_match(c,swd,NULL,0,sc->flags);
    if (r != 0)
    {
#if !defined(swd)
        free(swd);
#endif
        return r;
    }
    if (sc->max_chain > 0)
        swd->max_chain = sc->max_chain;
    if (sc->nice_length > 0)
        swd->nice_length = sc->nice_length;
    if (c->conf.max_match < swd->nice_length)
        swd->nice_length = c->conf.max_match;

    // if (c->cb)
        // (*c->cb->callback)(0,0,-1,c->cb->user);

    c->last_m_off = 1;
    r = find_match(c,swd,0,0);
    if (r != 0)
        return r;
    while (c->look > 0)
    {
        u32 ahead;
        u32 max_ahead;
        int l1, l2;

        c->codesize = c->bb_op - out;

        m_len = c->m_len;
        m_off = c->m_off;

        assert(c->bp == c->ip - c->look);
        assert(c->bp >= in);
        if (lit == 0)
            ii = c->bp;
        assert(ii + lit == c->bp);
        assert(swd->b_char == *(c->bp));

        if (m_len < 2 || (m_len == 2 && (m_off > M2_MAX_OFFSET))
            || m_off > c->conf.max_offset)
        {
            /* a literal */
            lit++;
            swd->max_chain = sc->max_chain;
            r = find_match(c,swd,1,0);
            assert(r == 0);
            continue;
        }

    /* a match */
#if defined(SWD_BEST_OFF)
        if (swd->use_best_off)
            better_match(swd,&m_len,&m_off);
#endif
        assert_match(swd,m_len,m_off);

        /* shall we try a lazy match ? */
        ahead = 0;
        if (sc->try_lazy <= 0 || m_len >= sc->max_lazy || m_off == c->last_m_off)
        {
            /* no */
            l1 = 0;
            max_ahead = 0;
        }
        else
        {
            /* yes, try a lazy match */
            l1 = len_of_coded_match(c,m_len,m_off);
            assert(l1 > 0);
            max_ahead = MIN(sc->try_lazy, m_len - 1);
        }

        while (ahead < max_ahead && c->look > m_len)
        {
            if (m_len >= sc->good_length)
                swd->max_chain = sc->max_chain >> 2;
            else
                swd->max_chain = sc->max_chain;
            r = find_match(c,swd,1,0);
            ahead++;

            assert(r == 0);
            assert(c->look > 0);
            assert(ii + lit + ahead == c->bp);

            if (c->m_len < 2)
                continue;
#if defined(SWD_BEST_OFF)
            if (swd->use_best_off)
                better_match(swd,&c->m_len,&c->m_off);
#endif
            l2 = len_of_coded_match(c,c->m_len,c->m_off);
            if (l2 < 0)
                continue;
#if 1
            if (l1 + (int)(ahead + c->m_len - m_len) * 5 > l2 + (int)(ahead) * 9)
#else
            if (l1 > l2)
#endif
            {
                c->lazy++;
                assert_match(swd,c->m_len,c->m_off);

#if 0
                if (l3 > 0)
                {
                    /* code previous run */
                    code_run(c,ii,lit);
                    lit = 0;
                    /* code shortened match */
                    code_match(c,ahead,m_off);
                }
                else
#endif
                {
                    lit += ahead;
                    assert(ii + lit == c->bp);
                }
                goto lazy_match_done;
            }
        }

        assert(ii + lit + ahead == c->bp);

        /* 1 - code run */
        code_run(c,ii,lit);
        lit = 0;

        /* 2 - code match */
        code_match(c,m_len,m_off);
        swd->max_chain = sc->max_chain;
        r = find_match(c,swd,m_len,1+ahead);
        assert(r == 0);

lazy_match_done: ;
    }

    /* store final run */
    code_run(c,ii,lit);

    /* EOF */
    bbPutBit(c, 0);
    code_prefix_ss12(c, (0x1000000));
    bbPutByte(c, 0xff);
    bbFlushBits(c, 0);

    assert(c->textsize == in_len);
    c->codesize = c->bb_op - out;
    *out_len = c->bb_op - out;
    // if (c->cb)
        // (*c->cb->callback)(c->textsize,c->codesize,4,c->cb->user);

#if 0
    printf("%7ld %7ld -> %7ld   %7ld %7ld   %ld  (max: %d %d %d)\n",
          (long) c->textsize, (long) in_len, (long) c->codesize,
           c->match_bytes, c->lit_bytes,  c->lazy,
           c->result[1], c->result[3], c->result[5]);
#endif
    assert(c->lit_bytes + c->match_bytes == in_len);

    swd_exit(swd);
#if !defined(swd)
    free(swd);
#endif
    return 0;
#undef swd
}
