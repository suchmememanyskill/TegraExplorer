#include "types.h"

#include <string.h>

#ifndef SWD_H
#define SWD_H

#define HEAD3(b,p) (((0x9f5f*(((((u32)b[p]<<5)^b[p+1])<<5)^b[p+2]))>>5) & (HSIZE-1))
#define s_head3(s,key)        s->head3[key]
#define swd_pos2off(s,pos) (s->bp > (pos) ? s->bp - (pos) : s->b_size - ((pos) - s->bp))
#define getbyte(c)  ((c).ip < (c).in_end ? (unsigned) *((c).ip)++ : (-1))

void swd_initdict(swd_t *s, const byte *dict, u32 dict_len);
void swd_insertdict(swd_t *s, u32 node, u32 len);
int  swd_init(swd_t *s, const byte *dict, u32 dict_len);
void swd_exit(swd_t *s);
void swd_remove_node(swd_t *s, u32 node);
void swd_getbyte(swd_t *s);
void swd_accept(swd_t *s, u32 n);
void swd_search(swd_t *s, u32 node, u32 cnt);
void swd_findbest(swd_t *s);

#endif
