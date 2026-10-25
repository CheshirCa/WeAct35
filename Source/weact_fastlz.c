/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Independent implementation from the public FastLZ level-1 format:
 * https://github.com/ariya/FastLZ (MIT). No upstream implementation copied.
 * Literals 1..32 bytes; distances 1..8192; matches 3..264 bytes.
 */
#include "weact_fastlz.h"
#include <string.h>
static unsigned hash3(const uint8_t *p)
{return ((unsigned)p[0]*251u+(unsigned)p[1]*31u+p[2])&2047u;}
static void literals(const uint8_t *src,size_t a,size_t b,uint8_t *out,size_t *used)
{
    while(a<b){size_t n=b-a;if(n>32)n=32;out[(*used)++]=(uint8_t)(n-1);
        memcpy(out+*used,src+a,n);*used+=n;a+=n;}
}
wx_status_t wx_fastlz_encode(const uint8_t *src,size_t n,uint8_t *out,size_t cap,size_t *written)
{
    if(written)*written=0;
    if(!src || !out || !written || !n || n>WX_FASTLZ_BLOCK_SIZE)return WX_INVALID;
    if(cap<n+(n+31)/32)return WX_BOUNDS;
    uint16_t table[2048];for(size_t k=0;k<2048;++k)table[k]=UINT16_MAX;
    size_t pos=0,anchor=0,used=0;
    /* A first literal and four trailing literals keep streams compatible with
     * classic FastLZ decoders as well as the current reference implementation. */
    while(pos+7<n) {
        unsigned h=hash3(src+pos);size_t ref=table[h];table[h]=(uint16_t)pos;
        size_t len=0;
        if(pos && ref<pos && pos-ref<=8192 && !memcmp(src+ref,src+pos,3)) {
            len=3;while(len<264 && pos+len<n-4 && src[ref+len]==src[pos+len])++len;
        }
        if(len<3){++pos;continue;}
        literals(src,anchor,pos,out,&used);
        size_t distance=pos-ref-1;
        if(len<9)out[used++]=(uint8_t)(((len-2)<<5)|(distance>>8));
        else {out[used++]=(uint8_t)(0xe0|(distance>>8));out[used++]=(uint8_t)(len-9);}
        out[used++]=(uint8_t)distance;
        size_t end=pos+len;
        for(++pos;pos<end;++pos)if(pos+2<n)table[hash3(src+pos)]=(uint16_t)pos;
        anchor=pos;
    }
    literals(src,anchor,n,out,&used);*written=used;return WX_OK;
}
