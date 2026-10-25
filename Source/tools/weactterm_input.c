/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 * Bounded streaming SSH keyboard decoder, not a terminal output parser.
 */
#include "weactterm_input.h"
#include <string.h>
static void finish_escape(wt_input *in,wt_screen *s)
{
    static const struct {const char *seq;VTermKey key;} keys[]={
        {"\033[A",VTERM_KEY_UP},{"\033[B",VTERM_KEY_DOWN},{"\033[C",VTERM_KEY_RIGHT},{"\033[D",VTERM_KEY_LEFT},
        {"\033OA",VTERM_KEY_UP},{"\033OB",VTERM_KEY_DOWN},{"\033OC",VTERM_KEY_RIGHT},{"\033OD",VTERM_KEY_LEFT},
        {"\033[H",VTERM_KEY_HOME},{"\033[F",VTERM_KEY_END},{"\033OH",VTERM_KEY_HOME},{"\033OF",VTERM_KEY_END},
        {"\033[1~",VTERM_KEY_HOME},{"\033[4~",VTERM_KEY_END},{"\033[2~",VTERM_KEY_INS},{"\033[3~",VTERM_KEY_DEL},
        {"\033[5~",VTERM_KEY_PAGEUP},{"\033[6~",VTERM_KEY_PAGEDOWN},
        {"\033OP",VTERM_KEY_FUNCTION(1)},{"\033OQ",VTERM_KEY_FUNCTION(2)},
        {"\033OR",VTERM_KEY_FUNCTION(3)},{"\033OS",VTERM_KEY_FUNCTION(4)}
    };
    if(in->length==1)vterm_keyboard_key(s->vt,VTERM_KEY_ESCAPE,VTERM_MOD_NONE);
    else {
        bool found=false;
        for(size_t n=0;n<sizeof(keys)/sizeof(keys[0]);++n)
            if(strlen(keys[n].seq)==in->length && !memcmp(keys[n].seq,in->escape,in->length)) {
                vterm_keyboard_key(s->vt,keys[n].key,VTERM_MOD_NONE);found=true;break;
            }
        if(!found)wt_screen_queue(s,in->escape,in->length); /* Preserve unrecognised keyboard combinations. */
    }
    in->length=0;
}
void wt_input_tick(wt_input *in,wt_screen *s,uint64_t now)
{if(in->length && now-in->escape_since>=40)finish_escape(in,s);}
static void plain(wt_input *in,wt_screen *s,unsigned char b)
{
    if(in->needed) {
        if((b&0xc0)!=0x80) {
            in->needed=0;vterm_keyboard_unichar(s->vt,0xfffd,VTERM_MOD_NONE);plain(in,s,b);return;
        }
        in->code=(in->code<<6)|(b&63);
        if(--in->needed)return;
        uint32_t c=in->code;
        if(c<in->min || c>0x10ffff || (c>=0xd800 && c<=0xdfff))c=0xfffd;
        vterm_keyboard_unichar(s->vt,c,VTERM_MOD_NONE);return;
    }
    if(b>=0x80) {
        if(b>=0xc2 && b<=0xdf){in->needed=1;in->code=b&31;in->min=0x80;}
        else if(b>=0xe0 && b<=0xef){in->needed=2;in->code=b&15;in->min=0x800;}
        else if(b>=0xf0 && b<=0xf4){in->needed=3;in->code=b&7;in->min=0x10000;}
        else vterm_keyboard_unichar(s->vt,0xfffd,VTERM_MOD_NONE);
    } else if(b=='\r' || b=='\n')vterm_keyboard_key(s->vt,VTERM_KEY_ENTER,VTERM_MOD_NONE);
    else if(b==127 || b==8)vterm_keyboard_key(s->vt,VTERM_KEY_BACKSPACE,VTERM_MOD_NONE);
    else if(b==9)vterm_keyboard_key(s->vt,VTERM_KEY_TAB,VTERM_MOD_NONE);
    else if(b<32) {char c=(char)b;wt_screen_queue(s,&c,1);}
    else vterm_keyboard_unichar(s->vt,b,VTERM_MOD_NONE);
}
void wt_input_bytes(wt_input *in,wt_screen *s,const char *p,size_t n,uint64_t now)
{
    wt_input_tick(in,s,now);
    for(size_t k=0;k<n && !in->stop;++k) {
        unsigned char b=(unsigned char)p[k];
        if(b==29 || (b==3 && !in->ctrl_c_to_pty)){in->stop=true;break;}
        if(in->length) {
            in->escape[in->length++]=(char)b;
            if(in->length==sizeof(in->escape) ||
               (in->length==2 && b!='[' && b!='O') ||
               (in->length>=3 && b>=0x40 && b<=0x7e))finish_escape(in,s);
        } else if(b==27) {in->escape[0]=27;in->length=1;in->escape_since=now;}
        else plain(in,s,b);
    }
}
