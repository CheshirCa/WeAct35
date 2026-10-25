/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Pure host encoding, MIT; third-party declarations/tables have notices below.
 */
#include "weact_codes.h"
#include "codes/qrencode_abi.h"
#include "codes/linear_tables.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct wx_code {
    wx_code_info_t info;
    wx_code_allocator_t allocator;
    uint8_t *modules;
};
static void *allocate(const wx_code_allocator_t *a, size_t n)
{ return a && a->allocate ? a->allocate(a->context, n) : malloc(n); }
static void release(const wx_code_allocator_t *a, void *p)
{ if (p) { if (a && a->release) a->release(a->context, p); else free(p); } }
void wx_code_destroy(wx_code_t *c)
{
    if (c) { wx_code_allocator_t a=c->allocator; release(&a,c->modules); release(&a,c); }
}
wx_code_status_t wx_code_info(const wx_code_t *c, wx_code_info_t *out)
{ if (!c || !out) return WX_CODE_INVALID; *out=c->info; return WX_CODE_OK; }
const uint8_t *wx_code_modules(const wx_code_t *c, size_t *length)
{
    if (length) *length=c ? c->info.height*c->info.stride : 0;
    return c ? c->modules : NULL;
}
static wx_code_status_t create(wx_code_format_t format, size_t w, size_t h,
    const wx_code_allocator_t *a, wx_code_t **out)
{
    if (!w || !h || w>SIZE_MAX/h) return WX_CODE_TOO_LONG;
    wx_code_t *c=allocate(a,sizeof(*c));
    if (!c) return WX_CODE_NOMEM;
    memset(c,0,sizeof(*c));
    if (a) c->allocator=*a;
    c->modules=allocate(a,w*h);
    if (!c->modules) { release(a,c); return WX_CODE_NOMEM; }
    memset(c->modules,0,w*h);
    c->info=(wx_code_info_t){.format=format,.width=w,.height=h,.stride=w,.check_digit=-1};
    *out=c;
    return WX_CODE_OK;
}
/* Strict RFC 3629 validation, permits U+0000 and rejects overlong encodings,
 * surrogates and code points above U+10FFFF. No decoding/normalization. */
static int utf8_valid(const uint8_t *s, size_t n)
{
    for (size_t i=0;i<n;) {
        uint32_t cp=s[i++], minimum=0; unsigned count=0;
        if (cp<0x80) continue;
        if (cp>=0xc2 && cp<=0xdf) { cp&=0x1f; count=1; minimum=0x80; }
        else if (cp>=0xe0 && cp<=0xef) { cp&=0x0f; count=2; minimum=0x800; }
        else if (cp>=0xf0 && cp<=0xf4) { cp&=7; count=3; minimum=0x10000; }
        else return 0;
        if (count>n-i) return 0;
        while (count--) { unsigned b=s[i++]; if ((b&0xc0)!=0x80) return 0; cp=(cp<<6)|(b&63); }
        if (cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    }
    return 1;
}
static wx_code_status_t qr_error(void)
{
    return errno==ENOMEM ? WX_CODE_NOMEM : errno==ERANGE ? WX_CODE_TOO_LONG : WX_CODE_BACKEND;
}
static wx_code_status_t qr(const uint8_t *s, size_t n, const wx_qr_options_t *options,
    const wx_code_allocator_t *a, wx_code_t **out)
{
    wx_qr_options_t o=options ? *options : (wx_qr_options_t){WX_QR_M,1,40};
    if (o.ecc<WX_QR_L || o.ecc>WX_QR_H || !o.min_version ||
        o.max_version>40 || o.min_version>o.max_version) return WX_CODE_INVALID;
    if (n>2953) return WX_CODE_TOO_LONG;
    if (!utf8_valid(s,n)) return WX_CODE_CHARACTER;
    int major,minor,micro;
    QRcode_APIVersion(&major,&minor,&micro);
    if (major!=4 || minor<1) return WX_CODE_BACKEND;
    errno=0;
    QRinput *input=QRinput_new2((int)o.min_version,(QRecLevel)o.ecc);
    if (!input) return qr_error();
    if (QRinput_appendECIheader(input,26)<0 || QRinput_append(input,QR_MODE_8,(int)n,s)<0) {
        wx_code_status_t status=qr_error(); QRinput_free(input); return status;
    }
    QRcode *raw=QRcode_encodeInput(input);
    wx_code_status_t status=raw ? WX_CODE_OK : qr_error();
    QRinput_free(input);
    if (!raw) return status;
    if (raw->version<(int)o.min_version || raw->version>40 || raw->width!=17+4*raw->version || !raw->data)
        status=WX_CODE_BACKEND;
    else if ((unsigned)raw->version>o.max_version) status=WX_CODE_TOO_LONG;
    else {
        status=create(WX_CODE_QR,(size_t)raw->width,(size_t)raw->width,a,out);
        if (status==WX_CODE_OK) {
            wx_code_t *c=*out;
            c->info.qr_version=(unsigned)raw->version;
            c->info.quiet_left=c->info.quiet_right=c->info.quiet_top=c->info.quiet_bottom=4;
            for (size_t i=0;i<c->info.width*c->info.height;i++) c->modules[i]=raw->data[i]&1;
        }
    }
    QRcode_free(raw);
    return status;
}
static void widths(wx_code_t *c, size_t *pos, const char *runs)
{
    unsigned black=1;
    for (;*runs;runs++,black^=1)
        for (int j=0;j<*runs-'0';j++) c->modules[(*pos)++]=(uint8_t)black;
}
static wx_code_status_t code39(const uint8_t *s,size_t n,const wx_code_allocator_t *a,wx_code_t **out)
{
    for (size_t i=0;i<n;i++) if (!s[i] || !strchr(c39_alphabet,s[i])) return WX_CODE_CHARACTER;
    /* 12 modules per character plus 1 module between characters. */
    wx_code_status_t status=create(WX_CODE_39,(n+2)*13-1,1,a,out);
    if (status!=WX_CODE_OK) return status;
    wx_code_t *c=*out; size_t pos=0;
    c->info.quiet_left=c->info.quiet_right=10;
    widths(c,&pos,c39_widths[43]); c->modules[pos++]=0;
    for (size_t i=0;i<n;i++) {
        widths(c,&pos,c39_widths[strchr(c39_alphabet,s[i])-c39_alphabet]);
        c->modules[pos++]=0;
    }
    widths(c,&pos,c39_widths[43]);
    return WX_CODE_OK;
}
static const char *const ean_l[10]={
    "0001101","0011001","0010011","0111101","0100011","0110001","0101111","0111011","0110111","0001011"};
static const char *const ean_parity[10]={
    "LLLLLL","LLGLGG","LLGGLG","LLGGGL","LGLLGG","LGGLLG","LGGGLL","LGLGLG","LGLGGL","LGGLGL"};
static void bits(wx_code_t *c,size_t *p,const char *s)
{ while (*s) c->modules[(*p)++]=(uint8_t)(*s++-'0'); }
static wx_code_status_t ean(wx_code_format_t f,const uint8_t *s,size_t n,
    const wx_code_allocator_t *a,wx_code_t **out)
{
    size_t total=f==WX_CODE_EAN8?8:13;
    if (n!=total && n!=total-1) return WX_CODE_INVALID;
    unsigned d[13],sum=0;
    for (size_t i=0;i<n;i++) { if (s[i]<'0'||s[i]>'9') return WX_CODE_CHARACTER; d[i]=s[i]-'0'; }
    for (size_t i=0;i<total-1;i++) sum+=d[i]*((total-2-i)%2?1:3);
    unsigned check=(10-sum%10)%10;
    if (n==total && d[total-1]!=check) return WX_CODE_CHECKSUM;
    d[total-1]=check;
    wx_code_status_t status=create(f,f==WX_CODE_EAN8?67:95,1,a,out);
    if (status!=WX_CODE_OK) return status;
    wx_code_t *c=*out; size_t pos=0;
    c->info.check_digit=(int)check;
    c->info.quiet_left=f==WX_CODE_EAN8?7:11; c->info.quiet_right=7;
    bits(c,&pos,"101");
    size_t left=total==8?4:6, first=total==8?0:1;
    for (size_t i=0;i<left;i++) {
        const char *pattern=ean_l[d[first+i]];
        int g=total==13 && ean_parity[d[0]][i]=='G';
        for (unsigned j=0;j<7;j++) c->modules[pos++]=(uint8_t)((pattern[g?6-j:j]-'0')^g);
    }
    bits(c,&pos,"01010");
    for (size_t i=first+left;i<total;i++)
        for (unsigned j=0;j<7;j++) c->modules[pos++]=(uint8_t)(1-(ean_l[d[i]][j]-'0'));
    bits(c,&pos,"101");
    return WX_CODE_OK;
}
enum { SET_A, SET_B, SET_C };
typedef struct { unsigned cost; uint8_t target,shift; } step_t;
static int value(unsigned set,const uint8_t *s,size_t n,size_t i)
{
    unsigned ch=s[i];
    if (set==SET_A) return ch<96 ? (int)(ch<32?ch+64:ch-32) : -1;
    if (set==SET_B) return ch>=32 && ch<128 ? (int)ch-32 : -1;
    return i+1<n && ch>='0' && ch<='9' && s[i+1]>='0' && s[i+1]<='9'
        ? (int)((ch-'0')*10+s[i+1]-'0') : -1;
}
static wx_code_status_t code128(const uint8_t *s,size_t n,const wx_code_allocator_t *a,wx_code_t **out)
{
    for (size_t i=0;i<n;i++) if (s[i]>127) return WX_CODE_CHARACTER;
    /* Bounded length at public entry makes allocation/products safe. */
    step_t (*dp)[3]=allocate(a,(n+1)*sizeof(*dp));
    if (!dp) return WX_CODE_NOMEM;
    memset(dp[n],0,sizeof(dp[n]));
    for (size_t i=n;i-->0;) for (unsigned set=0;set<3;set++) {
        step_t best={UINT_MAX/2,0,0};
        /* Prefer no latch on ties. Each candidate consumes data, so no cycles. */
        const unsigned order[4]={set,SET_B,SET_A,SET_C};
        for (unsigned k=0;k<4;k++) {
            unsigned target=order[k];
            if (value(target,s,n,i)<0) continue;
            size_t consumed=target==SET_C?2:1;
            unsigned cost=1+(target!=set)+dp[i+consumed][target].cost;
            if (cost<best.cost) best=(step_t){cost,(uint8_t)target,0};
        }
        if (set!=SET_C && value(1-set,s,n,i)>=0) {
            unsigned cost=2+dp[i+1][set].cost;
            if (cost<best.cost) best=(step_t){cost,(uint8_t)(1-set),1};
        }
        dp[i][set]=best;
    }
    unsigned set=SET_B;
    if (dp[0][SET_A].cost<dp[0][set].cost) set=SET_A;
    if (dp[0][SET_C].cost<dp[0][set].cost) set=SET_C;
    size_t count=1+dp[0][set].cost;
    wx_code_status_t status=create(WX_CODE_128,(count+1)*11+13,1,a,out);
    if (status!=WX_CODE_OK) { release(a,dp); return status; }
    wx_code_t *c=*out; size_t pos=0,weight=1;
    unsigned check=103+set;
    widths(c,&pos,c128_widths[check]);
    for (size_t i=0;i<n;) {
        step_t step=dp[i][set]; unsigned target=step.target;
        if (step.shift || target!=set) {
            unsigned symbol=step.shift?98:target==SET_C?99:target==SET_B?100:101;
            check=(check+symbol*weight++)%103; widths(c,&pos,c128_widths[symbol]);
        }
        unsigned symbol=(unsigned)value(target,s,n,i);
        check=(check+symbol*weight++)%103; widths(c,&pos,c128_widths[symbol]);
        i+=target==SET_C?2:1;
        if (!step.shift) set=target;
    }
    widths(c,&pos,c128_widths[check]); widths(c,&pos,c128_widths[106]);
    c->info.check_digit=(int)check; c->info.quiet_left=c->info.quiet_right=10;
    release(a,dp);
    return WX_CODE_OK;
}
wx_code_status_t wx_code_encode(wx_code_format_t format,const uint8_t *s,size_t n,
    const wx_qr_options_t *options,const wx_code_allocator_t *a,wx_code_t **out)
{
    if (!out || *out || !s || (a && (!!a->allocate!=!!a->release)) ||
        format<WX_CODE_QR || format>WX_CODE_39 || (options && format!=WX_CODE_QR)) return WX_CODE_INVALID;
    if (!n) return WX_CODE_EMPTY;
    if (format!=WX_CODE_QR && n>1024) return WX_CODE_TOO_LONG;
    switch (format) {
    case WX_CODE_QR: return qr(s,n,options,a,out);
    case WX_CODE_128: return code128(s,n,a,out);
    case WX_CODE_39: return code39(s,n,a,out);
    case WX_CODE_EAN8: case WX_CODE_EAN13: return ean(format,s,n,a,out);
    }
    return WX_CODE_INVALID;
}
