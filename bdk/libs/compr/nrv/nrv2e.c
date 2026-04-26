#define OK                    0
#define ERROR                 (-1)
#define INVALID_ARGUMENT      (-2)
#define OUT_OF_MEMORY         (-3)
/* compression errors */
#define NOT_COMPRESSIBLE      (-101)
/* decompression errors */
#define INPUT_OVERRUN         (-201)
#define OUTPUT_OVERRUN        (-202)
#define LOOKBEHIND_OVERRUN    (-203)
#define EOF_NOT_FOUND         (-204)
#define INPUT_NOT_CONSUMED    (-205)
#define OVERLAP_OVERRUN       (-206)

#include <utils/types.h>

#define fail(x,r)
#define getbit_8(bb, src, ilen) (((bb = bb & 0x7f ? bb*2 : ((unsigned)src[ilen++]*2+1)) >> 8) & 1)
#define getbit(bb)               getbit_8(bb, src, ilen)

int nrv2e_decompress_8(const u8 *src, u32 src_len, u8 *dst, u32* dst_len) {
    u32 bb = 0;
#ifdef TEST_OVERLAP
    u32 ilen = src_off, olen = 0, last_m_off = 1;
#else
    u32 ilen = 0, olen = 0, last_m_off = 1;
#endif

#ifdef TEST_OVERLAP
    src_len += src_off;
    fail(oend >= src_len, OVERLAP_OVERRUN);
#endif

    for (;;)
    {
        u32 m_off, m_len;

        while (getbit(bb))
        {
            fail(ilen >= src_len, INPUT_OVERRUN);
            fail(olen >= oend, OUTPUT_OVERRUN);
#ifdef TEST_OVERLAP
            fail(olen > ilen, OVERLAP_OVERRUN);
            olen++; ilen++;
#else
            dst[olen++] = src[ilen++];
#endif
        }
        m_off = 1;
        for (;;)
        {
            m_off = m_off*2 + getbit(bb);
            fail(ilen >= src_len, INPUT_OVERRUN);
            fail(m_off > UCL_UINT32_C(0xffffff) + 3, LOOKBEHIND_OVERRUN);
            if (getbit(bb)) break;
            m_off = (m_off-1)*2 + getbit(bb);
        }
        if (m_off == 2)
        {
            m_off = last_m_off;
            m_len = getbit(bb);
        }
        else
        {
            fail(ilen >= src_len, INPUT_OVERRUN);
            m_off = (m_off-3)*256 + src[ilen++];
            if (m_off == (0xffffffff))
                break;
            m_len = (m_off ^ (0xffffffff)) & 1;
            m_off >>= 1;
            last_m_off = ++m_off;
        }
        if (m_len)
            m_len = 1 + getbit(bb);
        else if (getbit(bb))
            m_len = 3 + getbit(bb);
        else
        {
            m_len++;
            do {
                m_len = m_len*2 + getbit(bb);
                fail(ilen >= src_len, INPUT_OVERRUN);
                fail(m_len >= oend, OUTPUT_OVERRUN);
            } while (!getbit(bb));
            m_len += 3;
        }
        m_len += (m_off > 0x500);
        fail(olen + m_len > oend, OUTPUT_OVERRUN);
        fail(m_off > olen, LOOKBEHIND_OVERRUN);
#ifdef TEST_OVERLAP
        olen += m_len + 1;
        fail(olen > ilen, OVERLAP_OVERRUN);
#else
        {
            const u8 *m_pos;
            m_pos = dst + olen - m_off;
            dst[olen++] = *m_pos++;
            do dst[olen++] = *m_pos++; while (--m_len > 0);
        }
#endif
    }
    *dst_len = olen;
    return ilen == src_len ? OK : (ilen < src_len ? INPUT_NOT_CONSUMED : INPUT_OVERRUN);
}
