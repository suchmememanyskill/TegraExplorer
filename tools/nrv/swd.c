#include "swd.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void swd_initdict(swd_t *s, const byte *dict, u32 dict_len)
{
    s->dict = s->dict_end = NULL;
    s->dict_len = 0;

    if (!dict || dict_len <= 0)
        return;
    if (dict_len > s->n)
    {
        dict += dict_len - s->n;
        dict_len = s->n;
    }

    s->dict = dict;
    s->dict_len = dict_len;
    s->dict_end = dict + dict_len;
    memcpy(s->b,dict,dict_len);
    s->ip = dict_len;
}

void swd_insertdict(swd_t *s, u32 node, u32 len)
{
    u32 key;

    s->node_count = s->n - len;
    s->first_rp = node;

    while (len-- > 0)
    {
        key = HEAD3(s->b,node);
        s->succ3[node] = s_head3(s,key);
        s->head3[key] = (node);
        s->best3[node] = (s->f + 1);
        s->llen3[key]++;
        assert(s->llen3[key] <= s->n);

#ifdef HEAD2
        key = HEAD2(s->b,node);
        s->head2[key] = SWD_UINT(node);
#endif

        node++;
    }
}

int swd_init(swd_t *s, const byte *dict, u32 dict_len)
{
    u32 i = 0;
    int c = 0;

    if (s->n == 0)
        s->n = N;
    if (s->f == 0)
        s->f = F;
    s->threshold = THRESHOLD;
    if (s->n > N || s->f > F)
        return -2;

#if defined(SWD_USE_MALLOC)
    s->b = (unsigned char *) malloc(s->n + s->f + s->f);
    s->head3 = (u32 *) malloc((SWD_HSIZE) * sizeof(*s->head3));
    s->succ3 = (u32 *) malloc((s->n + s->f) * sizeof(*s->succ3));
    s->best3 = (u32 *) malloc((s->n + s->f) * sizeof(*s->best3));
    s->llen3 = (u32 *) malloc((SWD_HSIZE) * sizeof(*s->llen3));
    if (!s->b || !s->head3  || !s->succ3 || !s->best3 || !s->llen3)
        return -3;
#ifdef HEAD2
    s->head2 = (u32 *) malloc(UCL_UINT32_C(SWD_HSIZE) * sizeof(*s->head2));
    if (!s->head2)
        return UCL_E_OUT_OF_MEMORY;
#endif
#endif

    /* defaults */
    s->max_chain = MAX_CHAIN;
    s->nice_length = s->f;
    s->use_best_off = 0;
    s->lazy_insert = 0;

    s->b_size = s->n + s->f;
    if (s->b_size + s->f >= 0xffffffffU)
        return -1;
    s->b_wrap = s->b + s->b_size;
    s->node_count = s->n;

    memset(s->llen3, 0, sizeof(s->llen3[0]) * HSIZE);
#ifdef HEAD2
#if 1
    ucl_memset(s->head2, 0xff, sizeof(s->head2[0]) * UCL_UINT32_C(SWD_HSIZE));
    assert(s->head2[0] == NIL2);
#else
    for (i = 0; i < UCL_UINT32_C(SWD_HSIZE); i++)
        s->head2[i] = NIL2;
#endif
#endif

    s->ip = 0;
    swd_initdict(s,dict,dict_len);
    s->bp = s->ip;
    s->first_rp = s->ip;

    assert(s->ip + s->f <= s->b_size);
#if 1
    s->look = (u32) (s->c->in_end - s->c->ip);
    if (s->look > 0)
    {
        if (s->look > s->f)
            s->look = s->f;
        memcpy(&s->b[s->ip],s->c->ip,s->look);
        s->c->ip += s->look;
        s->ip += s->look;
    }
#else
    s->look = 0;
    while (s->look < s->f)
    {
        if ((c = getbyte(*(s->c))) < 0)
            break;
        s->b[s->ip] = BYTE(c);
        s->ip++;
        s->look++;
    }
#endif
    if (s->ip == s->b_size)
        s->ip = 0;

    if (s->look >= 2 && s->dict_len > 0)
        swd_insertdict(s,0,s->dict_len);

    s->rp = s->first_rp;
    if (s->rp >= s->node_count)
        s->rp -= s->node_count;
    else
        s->rp += s->b_size - s->node_count;

#if defined(__UCL_CHECKER)
    /* initialize memory for the first few HEAD3 (if s->ip is not far
     * enough ahead to do this job for us). The value doesn't matter. */
    if (s->look < 3)
        ucl_memset(&s->b[s->bp+s->look],0,3);
#endif

    // UCL_UNUSED(i);
    // UCL_UNUSED(c);
    return 0;
}

void swd_exit(swd_t *s)
{
#if defined(SWD_USE_MALLOC)
    /* free in reverse order of allocations */
#ifdef HEAD2
    free(s->head2); s->head2 = NULL;
#endif
    free(s->llen3); s->llen3 = NULL;
    free(s->best3); s->best3 = NULL;
    free(s->succ3); s->succ3 = NULL;
    free(s->head3); s->head3 = NULL;
    free(s->b); s->b = NULL;
#else
    // UCL_UNUSED(s);
#endif
}

__inline__ void swd_remove_node(swd_t *s, u32 node)
{
    if (s->node_count == 0)
    {
        u32 key;

#ifdef UCL_DEBUG
        if (s->first_rp != UCL_UINT_MAX)
        {
            if (node != s->first_rp)
                printf("Remove %5d: %5d %5d %5d %5d  %6d %6d\n",
                        node, s->rp, s->ip, s->bp, s->first_rp,
                        s->ip - node, s->ip - s->bp);
            assert(node == s->first_rp);
            s->first_rp = UCL_UINT_MAX;
        }
#endif

        key = HEAD3(s->b,node);
        assert(s->llen3[key] > 0);
        --s->llen3[key];

#ifdef HEAD2
        key = HEAD2(s->b,node);
        assert(s->head2[key] != NIL2);
        if ((u32) s->head2[key] == node)
            s->head2[key] = NIL2;
#endif
    }
    else
        --s->node_count;
}

__inline__ void swd_getbyte(swd_t *s)
{
    int c;

    if ((c = getbyte(*(s->c))) < 0)
    {
        if (s->look > 0)
            --s->look;
#if defined(__UCL_CHECKER)
        /* initialize memory - value doesn't matter */
        s->b[s->ip] = 0;
        if (s->ip < s->f)
            s->b_wrap[s->ip] = 0;
#endif
    }
    else
    {
        s->b[s->ip] = (c);
        if (s->ip < s->f)
            s->b_wrap[s->ip] = (c);
    }
    if (++s->ip == s->b_size)
        s->ip = 0;
    if (++s->bp == s->b_size)
        s->bp = 0;
    if (++s->rp == s->b_size)
        s->rp = 0;
}

void swd_accept(swd_t *s, u32 n)
{
    assert(n <= s->look);

    if (n > 0) do
    {
        u32 key;

        swd_remove_node(s,s->rp);

        /* add bp into HEAD3 */
        key = HEAD3(s->b,s->bp);
        s->succ3[s->bp] = s_head3(s,key);
        s->head3[key] = (s->bp);
        s->best3[s->bp] = (s->f + 1);
        s->llen3[key]++;
        assert(s->llen3[key] <= s->n);

#ifdef HEAD2
        /* add bp into HEAD2 */
        key = HEAD2(s->b,s->bp);
        s->head2[key] = SWD_UINT(s->bp);
#endif

        swd_getbyte(s);
    } while (--n > 0);
}

void swd_search(swd_t *s, u32 node, u32 cnt)
{
#if 0 && defined(__GNUC__) && defined(__i386__)
    register const unsigned char *p1 __asm__("%edi");
    register const unsigned char *p2 __asm__("%esi");
    register const unsigned char *px __asm__("%edx");
#else
    const unsigned char *p1;
    const unsigned char *p2;
    const unsigned char *px;
#endif
    u32 m_len = s->m_len;
    const unsigned char * b  = s->b;
    const unsigned char * bp = s->b + s->bp;
    const unsigned char * bx = s->b + s->bp + s->look;
    unsigned char scan_end1;

    assert(s->m_len > 0);

    scan_end1 = bp[m_len - 1];
    for ( ; cnt-- > 0; node = s->succ3[node])
    {
        p1 = bp;
        p2 = b + node;
        px = bx;

        assert(m_len < s->look);

        if (
#if 1
            p2[m_len - 1] == scan_end1 &&
            p2[m_len] == p1[m_len] &&
#endif
            p2[0] == p1[0] &&
            p2[1] == p1[1])
        {
            u32 i;
            assert(memcmp(bp,&b[node],3) == 0);

#if 0 && defined(UCL_UNALIGNED_OK_4)
            p1 += 3; p2 += 3;
            while (p1 < px && * (const ucl_uint32p) p1 == * (const ucl_uint32p) p2)
                p1 += 4, p2 += 4;
            while (p1 < px && *p1 == *p2)
                p1 += 1, p2 += 1;
#else
            p1 += 2; p2 += 2;
            do {} while (++p1 < px && *p1 == *++p2);
#endif
            i = p1 - bp;

#ifdef UCL_DEBUG
            if (memcmp(bp,&b[node],i) != 0)
                printf("%5ld %5ld %02x%02x %02x%02x\n",
                        (long)s->bp, (long) node,
                        bp[0], bp[1], b[node], b[node+1]);
#endif
            assert(memcmp(bp,&b[node],i) == 0);

#if defined(SWD_BEST_OFF)
            if (i < SWD_BEST_OFF)
            {
                if (s->best_pos[i] == 0)
                    s->best_pos[i] = node + 1;
            }
#endif
            if (i > m_len)
            {
                s->m_len = m_len = i;
                s->m_pos = node;
                if (m_len == s->look)
                    return;
                if (m_len >= s->nice_length)
                    return;
                if (m_len > (u32) s->best3[node])
                    return;
                scan_end1 = bp[m_len - 1];
            }
        }
    }
}

void swd_findbest(swd_t *s)
{
    u32 key;
    u32 cnt, node;
    u32 len;

    assert(s->m_len > 0);

    /* get current head, add bp into HEAD3 */
    key = HEAD3(s->b,s->bp);
    node = s->succ3[s->bp] = s_head3(s,key);
    cnt = s->llen3[key]++;
    assert(s->llen3[key] <= s->n + s->f);
    if (cnt > s->max_chain && s->max_chain > 0)
        cnt = s->max_chain;
    s->head3[key] = (s->bp);

    s->b_char = s->b[s->bp];
    len = s->m_len;
    if (s->m_len >= s->look)
    {
        if (s->look == 0)
            s->b_char = -1;
        s->m_off = 0;
        s->best3[s->bp] = (s->f + 1);
    }
    else
    {
#ifdef HEAD2
        if (swd_search2(s))
#endif
            if (s->look >= 3)
                swd_search(s,node,cnt);
        if (s->m_len > len)
            s->m_off = swd_pos2off(s,s->m_pos);
        s->best3[s->bp] = (s->m_len);

#if defined(SWD_BEST_OFF)
        if (s->use_best_off)
        {
            int i;
            for (i = 2; i < SWD_BEST_OFF; i++)
                if (s->best_pos[i] > 0)
                    s->best_off[i] = swd_pos2off(s,s->best_pos[i]-1);
                else
                    s->best_off[i] = 0;
        }
#endif
    }

    swd_remove_node(s,s->rp);

#ifdef HEAD2
    /* add bp into HEAD2 */
    key = HEAD2(s->b,s->bp);
    s->head2[key] = SWD_UINT(s->bp);
#endif
}