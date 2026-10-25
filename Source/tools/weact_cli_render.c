/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Text/layout/image modes adapted from WeActCLI.pb with permission. GPL-3.0-only.
 */
#define _POSIX_C_SOURCE 200809L
#include "weact_cli.h"
#include "weact_cli_pixbuf.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int canvas_alloc(cli_canvas *c,int w,int h)
{
    if(w<=0 || h<=0 || (size_t)w>CLI_PIXELS_MAX/(size_t)h)return -1;
    *c=(cli_canvas){.width=w,.height=h};
    c->rgb=calloc((size_t)w*(size_t)h,3);c->mask=calloc((size_t)w*(size_t)h,1);
    if(!c->rgb || !c->mask){cli_canvas_free(c);return -1;}return 0;
}
void cli_canvas_free(cli_canvas *c){free(c->rgb);free(c->mask);*c=(cli_canvas){0};}
static int utf8(const char *text,uint32_t **out,size_t *length)
{
    size_t n=strlen(text),used=0;uint32_t *codes=calloc(n+1,sizeof(*codes));if(!codes)return -1;
    const unsigned char *p=(const unsigned char *)text;
    while(*p) {
        uint32_t cp;int follow;uint32_t min;
        if(*p<128){cp=*p++;follow=0;min=0;}
        else if(*p>=0xc2 && *p<=0xdf){cp=*p++&31;follow=1;min=128;}
        else if(*p>=0xe0 && *p<=0xef){cp=*p++&15;follow=2;min=2048;}
        else if(*p>=0xf0 && *p<=0xf4){cp=*p++&7;follow=3;min=65536;}
        else {free(codes);return -1;}
        for(int j=0;j<follow;j++){if((*p&0xc0)!=0x80){free(codes);return -1;}cp=(cp<<6)|(*p++&63);}
        if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)){free(codes);return -1;}
        codes[used++]=cp;
    }
    *out=codes;*length=used;return 0;
}
static int font_path(const char *font,char out[4096])
{
    if(*font=='/' && strlen(font)<4096 && access(font,R_OK)==0){strcpy(out,font);return 0;}
    int pipes[2];if(pipe(pipes)<0)return -1;
    pid_t pid=fork();if(pid<0){close(pipes[0]);close(pipes[1]);return -1;}
    if(!pid) {
        close(pipes[0]);if(dup2(pipes[1],STDOUT_FILENO)<0)_exit(127);close(pipes[1]);
        execlp("fc-match","fc-match","--format=%{file}","--",font,(char *)NULL);_exit(127);
    }
    close(pipes[1]);size_t used=0;int failed=0;
    for(;;) {
        struct pollfd p={.fd=pipes[0],.events=POLLIN};
        if(poll(&p,1,2000)<=0){failed=1;break;}
        ssize_t n=read(pipes[0],out+used,4095-used);
        if(n<0){if(errno==EINTR)continue;failed=1;break;}
        if(!n)break;
        used+=(size_t)n;if(used==4095){failed=1;break;}
    }
    close(pipes[0]);if(failed)kill(pid,SIGTERM);
    int status=0;while(waitpid(pid,&status,0)<0)if(errno!=EINTR){failed=1;break;}
    out[used]=0;
    if(failed || !WIFEXITED(status) || WEXITSTATUS(status) || !used)
        strcpy(out,"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    return 0;
}
static int advance(FT_Face face,uint32_t cp)
{
    if(FT_Load_Char(face,cp,FT_LOAD_DEFAULT))return 1;
    int n=(int)(face->glyph->advance.x>>6);return n>4096?4096:n>0?n:0;
}
typedef struct {size_t start,end;int width;} text_line;
static int text_canvas(cli_options *o,int w,cli_canvas *c)
{
    uint32_t *codes=NULL;size_t count=0;
    if(utf8(o->text,&codes,&count))return -1;
    FT_Library ft=NULL;FT_Face face=NULL;char path[4096];int result=-1,fontfd=-1;
    unsigned char *fontdata=NULL;size_t fontbytes=0;
    if(font_path(o->font,path))goto done;
    fontfd=cli_regular_open(path,32u*1024u*1024u,&fontbytes);if(fontfd<0)goto done;
    fontdata=malloc(fontbytes);if(!fontdata)goto done;
    for(size_t used=0;used<fontbytes;) {
        ssize_t n=read(fontfd,fontdata+used,fontbytes-used);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)goto done;
        used+=(size_t)n;
    }
    if(close(fontfd)<0){fontfd=-1;goto done;}fontfd=-1;
    if(FT_Init_FreeType(&ft) || FT_New_Memory_Face(ft,fontdata,(FT_Long)fontbytes,0,&face))goto done;
    int wrap=w-(o->center?20:10);if(wrap<=0)goto done;
    int size=o->size;
    for(;;) {
        if(FT_Set_Pixel_Sizes(face,0,(unsigned)size))goto done;
        int word=0,longest=0;
        for(size_t k=0;k<=count;k++) {
            if(k==count || codes[k]==' ' || codes[k]=='\n'){if(word>longest)longest=word;word=0;}
            else word+=advance(face,codes[k]);
        }
        if(longest<=wrap || size==6)break;
        size--;
    }
    text_line *lines=calloc(count+1,sizeof(*lines));if(!lines)goto done;
    size_t nlines=0,start=0;
    while(start<count) {
        size_t at=start,space=SIZE_MAX;int width=0;
        while(at<count && codes[at]!='\n') {
            int a=advance(face,codes[at]);if(width+a>wrap && at>start)break;
            width+=a;if(codes[at]==' ')space=at;at++;
        }
        size_t end=at,next=at;
        if(at<count && codes[at]=='\n')next=at+1;
        else if(at<count && space!=SIZE_MAX && space>start){end=space;next=space+1;}
        if(next==start)next=++end;
        width=0;for(size_t k=start;k<end;k++)width+=advance(face,codes[k]);
        lines[nlines++]=(text_line){start,end,width};start=next;
    }
    int line_height=size+2;
    if(!nlines || nlines>(size_t)INT32_MAX/(unsigned)line_height || canvas_alloc(c,w,(int)nlines*line_height)) {free(lines);goto done;}
    int asc=(int)(face->size->metrics.ascender>>6);
    for(size_t row=0;row<nlines;row++) {
        int x=o->x>=0?o->x:o->center?(w-lines[row].width)/2:5;if(x<0)x=0;
        for(size_t k=lines[row].start;k<lines[row].end;k++) {
            if(FT_Load_Char(face,codes[k],FT_LOAD_RENDER))continue;
            FT_GlyphSlot g=face->glyph;FT_Bitmap *b=&g->bitmap;
            if(b->width && b->rows &&
               (!b->buffer || b->width>4096 || b->rows>4096 || b->pitch==INT32_MIN ||
                (b->pixel_mode!=FT_PIXEL_MODE_MONO && b->pixel_mode!=FT_PIXEL_MODE_GRAY) ||
                (unsigned)(b->pitch<0?-b->pitch:b->pitch)<(b->pixel_mode==FT_PIXEL_MODE_MONO?(b->width+7)/8:b->width))) {
                free(lines);cli_canvas_free(c);goto done;
            }
            for(unsigned by=0;by<b->rows;by++)for(unsigned bx=0;bx<b->width;bx++) {
                int px=x+g->bitmap_left+(int)bx,py=(int)row*line_height+asc-g->bitmap_top+(int)by;
                if(px<0 || px>=w || py<0 || py>=c->height)continue;
                int pitch=b->pitch;size_t byoff=pitch>=0?(size_t)by*(unsigned)pitch:(size_t)(b->rows-1-by)*(unsigned)(-pitch);
                unsigned alpha=b->pixel_mode==FT_PIXEL_MODE_MONO?((b->buffer[byoff+bx/8]&(128>>(bx%8)))?255:0):b->buffer[byoff+bx];
                size_t at=(size_t)py*(unsigned)w+(unsigned)px;
                if(o->overlay)alpha=alpha>=128?255:0;
                if(alpha>c->mask[at]) {
                    c->mask[at]=(unsigned char)alpha;
                    for(int j=0;j<3;j++)c->rgb[at*3+(unsigned)j]=(unsigned char)(o->color[j]*alpha/255);
                }
            }
            x+=(int)(g->advance.x>>6);
        }
    }
    free(lines);if(o->verbose)fprintf(stderr,"font=%s pixels=%d lines=%zu\n",path,size,nlines);result=0;
done:
    if(face)FT_Done_Face(face);
    if(ft)FT_Done_FreeType(ft);
    if(fontfd>=0)close(fontfd);
    free(fontdata);free(codes);return result;
}
typedef struct {int invalid;} decode_limit;
static void image_size(GdkPixbufLoader *loader,int w,int h,void *data)
{
    decode_limit *limit=data;
    if(w<=0 || h<=0 || w>4096 || h>4096 || (size_t)w>CLI_PIXELS_MAX/(unsigned)h) {
        limit->invalid=1;gdk_pixbuf_loader_set_size(loader,1,1);
    }
}
static int gif_blocks(const unsigned char *p,size_t n,size_t *at)
{
    while(*at<n) {size_t bytes=p[(*at)++];if(!bytes)return 0;if(bytes>n-*at)return -1;*at+=bytes;}
    return -1;
}
static int gif_first(unsigned char *p,size_t *length)
{
    size_t n=*length;if(n<13)return -1;
    unsigned logical_w=p[6]|(unsigned)p[7]<<8,logical_h=p[8]|(unsigned)p[9]<<8;
    if(!logical_w || !logical_h || logical_w>4096 || logical_h>4096)return -1;
    size_t at=13;if(p[10]&128)at+=3u<<(1+(p[10]&7));
    while(at<n) {
        unsigned tag=p[at++];
        if(tag==0x21){if(at==n)return -1;at++;if(gif_blocks(p,n,&at))return -1;}
        else if(tag==0x2c) {
            if(n-at<9)return -1;
            unsigned frame_w=p[at+4]|(unsigned)p[at+5]<<8,frame_h=p[at+6]|(unsigned)p[at+7]<<8;
            if(!frame_w || !frame_h || frame_w>4096 || frame_h>4096)return -1;
            unsigned packed=p[at+8];at+=9;
            if(packed&128)at+=3u<<(1+(packed&7));
            if(at>=n)return -1;
            at++;
            if(gif_blocks(p,n,&at))return -1;
            p[at++]=0x3b;*length=at;return 0;
        } else return -1;
    }
    return -1;
}
static unsigned be16(const unsigned char *p){return (unsigned)p[0]*256+p[1];}
static uint32_t be32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint32_t le32(const unsigned char *p){return (uint32_t)p[3]<<24|(uint32_t)p[2]<<16|(uint32_t)p[1]<<8|p[0];}
static int image_dimensions_ok(unsigned w,unsigned h)
{return w && h && w<=4096 && h<=4096 && (size_t)w<=CLI_PIXELS_MAX/h;}
/* Reject oversized dimensions independently of lazy decoder allocation.
 * Only the four promised file formats reach GdkPixbuf. */
static int image_preflight(unsigned char *p,size_t *length)
{
    size_t n=*length;
    if(n>=8 && !memcmp(p,"\x89PNG\r\n\x1a\n",8))
        return n>=33 && be32(p+8)==13 && !memcmp(p+12,"IHDR",4) && image_dimensions_ok(be32(p+16),be32(p+20))?0:-1;
    if(n>=6 && (!memcmp(p,"GIF87a",6) || !memcmp(p,"GIF89a",6)))return gif_first(p,length);
    if(n>=26 && p[0]=='B' && p[1]=='M') {
        uint32_t dib=le32(p+14);unsigned w,h;
        if(dib==12) {w=(unsigned)p[18]|(unsigned)p[19]<<8;h=(unsigned)p[20]|(unsigned)p[21]<<8;}
        else {
            if(n<54 || dib<40 || dib>n-14)return -1;
            int32_t sw=(int32_t)le32(p+18),sh=(int32_t)le32(p+22);
            if(sw<=0 || sh==INT32_MIN || !sh || le32(p+30)>3)return -1;
            w=(unsigned)sw;h=(unsigned)(sh<0?-sh:sh);
        }
        return image_dimensions_ok(w,h)?0:-1;
    }
    if(n>=4 && p[0]==255 && p[1]==216) {
        size_t at=2;
        while(at<n) {
            if(p[at++]!=255)return -1;
            while(at<n && p[at]==255)at++;
            if(at==n)return -1;
            unsigned tag=p[at++];
            if(tag==0xda || tag==0xd9)return -1;
            if(tag==0xd8 || tag==1 || (tag>=0xd0 && tag<=0xd7))continue;
            if(n-at<2)return -1;
            unsigned bytes=be16(p+at);if(bytes<2 || bytes>n-at)return -1;
            if(tag>=0xc0 && tag<=0xcf && tag!=0xc4 && tag!=0xc8 && tag!=0xcc) {
                if(bytes<8 || p[at+2]!=8)return -1;
                return image_dimensions_ok(be16(p+at+5),be16(p+at+3))?0:-1;
            }
            at+=bytes;
        }
    }
    return -1;
}
static double cubic(double t)
{
    t=fabs(t);if(t<1)return 1.5*t*t*t-2.5*t*t+1;
    if(t<2)return -0.5*t*t*t+2.5*t*t-4*t+2;
    return 0;
}
static int image_canvas(cli_options *o,int w,int h,cli_canvas *c)
{
    size_t length=0;int fd=cli_regular_open(o->image,CLI_IMAGE_MAX,&length);if(fd<0)return -1;
    GdkPixbufLoader *loader=gdk_pixbuf_loader_new();if(!loader){close(fd);return -1;}
    decode_limit limit={0};g_signal_connect_data(loader,"size-prepared",(void (*)(void))image_size,&limit,NULL,0);
    size_t used=0;
    unsigned char *raw=malloc(length+1);int failed=!raw;
    while(!failed && used<length) {
        ssize_t n=read(fd,raw+used,length-used);
        if(n<0){if(errno==EINTR)continue;failed=1;break;}
        if(!n){failed=1;break;}used+=(size_t)n;
    }
    unsigned char extra;
    if(!failed && read(fd,&extra,1)!=0)failed=1;
    if(close(fd)<0)failed=1;
    if(!failed && image_preflight(raw,&length))failed=1;
    for(size_t at=0;!failed && at<length;) {
        size_t amount=length-at;if(amount>4096)amount=4096;
        if(!gdk_pixbuf_loader_write(loader,raw+at,amount,NULL) || limit.invalid)failed=1;
        at+=amount;
    }
    free(raw);
    if(!gdk_pixbuf_loader_close(loader,NULL) || limit.invalid)failed=1;
    GdkPixbuf *pix=failed?NULL:gdk_pixbuf_loader_get_pixbuf(loader);int result=-1;
    if(!pix)goto done;
    int sw=gdk_pixbuf_get_width(pix),sh=gdk_pixbuf_get_height(pix),channels=gdk_pixbuf_get_n_channels(pix),stride=gdk_pixbuf_get_rowstride(pix);
    if(sw<=0 || sh<=0 || sw>4096 || sh>4096 || channels<3 || channels>4 || gdk_pixbuf_get_bits_per_sample(pix)!=8 || stride<sw*channels)goto done;
    int tw=sw,th=sh;
    if(o->mode==0) {double scale=fmin((double)w/sw,(double)h/sh);tw=(int)(sw*scale);th=(int)(sh*scale);}
    else if(o->iw>0 && o->ih>0){tw=o->iw;th=o->ih;}
    if(tw>w)tw=w;
    if(th>h)th=h;
    if(tw<1)tw=1;
    if(th<1)th=1;
    if(canvas_alloc(c,w,h))goto done;
    int x=o->mode==1?0:(w-tw)/2,y=o->mode==1?0:(h-th)/2;
    const unsigned char *source=gdk_pixbuf_get_pixels(pix);
    for(int j=0;j<th;j++)for(int i=0;i<tw;i++) {
        double sx=((i+0.5)*sw/tw)-0.5,sy=((j+0.5)*sh/th)-0.5;
        if(sx<0)sx=0;
        if(sy<0)sy=0;
        int ix=(int)sx,iy=(int)sy,nx=ix+1<sw?ix+1:ix,ny=iy+1<sh?iy+1:iy;
        double fx=sx-ix,fy=sy-iy;unsigned char color[3];
        for(int ch=0;ch<3;ch++) {
            double values[4];int xs[]={ix,nx},ys[]={iy,ny};
            for(int a=0;a<2;a++)for(int b=0;b<2;b++) {
                const unsigned char *p=source+(size_t)ys[a]*(unsigned)stride+(size_t)xs[b]*(unsigned)channels;
                values[a*2+b]=p[ch]*(channels==4?p[3]/255.0:1.0);
            }
            double v;
            if(o->quality==0 || o->quality==3) {
                int xx=(int)(sx+0.5),yy=(int)(sy+0.5);if(xx>=sw)xx=sw-1;if(yy>=sh)yy=sh-1;
                const unsigned char *p=source+(size_t)yy*(unsigned)stride+(size_t)xx*(unsigned)channels;
                v=p[ch]*(channels==4?p[3]/255.0:1.0);
            } else if(o->quality==2 || o->quality==4) {
                v=0;
                for(int a=-1;a<=2;a++)for(int b=-1;b<=2;b++) {
                    int xx=ix+b,yy=iy+a;double weight=cubic(sx-(ix+b))*cubic(sy-(iy+a));
                    if(xx<0)xx=0;
                    if(xx>=sw)xx=sw-1;
                    if(yy<0)yy=0;
                    if(yy>=sh)yy=sh-1;
                    const unsigned char *p=source+(size_t)yy*(unsigned)stride+(size_t)xx*(unsigned)channels;
                    v+=weight*p[ch]*(channels==4?p[3]/255.0:1.0);
                }
            } else v=(1-fy)*((1-fx)*values[0]+fx*values[1])+fy*((1-fx)*values[2]+fx*values[3]);
            if(v<0)v=0;
            if(v>255)v=255;
            color[ch]=(unsigned char)v;
        }
        if(o->quality>=3) {
            int gray=(77*color[0]+150*color[1]+29*color[2])>>8;
            unsigned char bw=gray>=128?255:0;for(int ch=0;ch<3;ch++)color[ch]=bw;
        }
        size_t at=(size_t)(y+j)*(unsigned)w+(unsigned)(x+i);memcpy(c->rgb+at*3,color,3);c->mask[at]=255;
    }
    result=0;
done:g_object_unref(loader);return result;
}
int cli_canvas_make(cli_options *o,int w,int h,cli_canvas *c)
{
    *c=(cli_canvas){0};if(o->cls)return canvas_alloc(c,w,h);
    if(o->overlay && (o->scroll || *o->image)){fputs("Overlay with scroll/image requires unknown background; unsupported\n",stderr);return -1;}
    return *o->image?image_canvas(o,w,h,c):text_canvas(o,w,c);
}
int cli_frame(const cli_canvas *c,wx_surface_t *s,int offset)
{
    wx_surface_info_t i;if(wx_surface_info(s,&i)!=WX_OK)return -1;
    if(wx_surface_fill_rgb(s,0,0,i.width,i.height,0,0,0)!=WX_OK)return -1;
    for(size_t row=0;row<i.height;row++) {
        int source=(int)row-offset;if(source<0 || source>=c->height)continue;
        if(wx_surface_bitmap_rgb(s,0,row,i.width,1,c->rgb+(size_t)source*(unsigned)c->width*3,(size_t)c->width*3,0)!=WX_OK)return -1;
    }
    return 0;
}
/* Packed text is converted once with the existing profile codec. Every frame
 * copies only visible rows; black padding and old glyph erasure are explicit. */
int cli_packed_frame(const unsigned char *packed,size_t stride,int height,wx_surface_t *s,int offset)
{
    wx_surface_info_t i;size_t length;
    if(!packed || height<=0 || wx_surface_info(s,&i)!=WX_OK ||
       i.width>SIZE_MAX/2 || stride!=i.width*2 || i.stride!=stride ||
       i.height>SIZE_MAX/stride || (size_t)height>SIZE_MAX/stride)return -1;
    unsigned char *pixels=wx_surface_pixels(s,&length);
    if(!pixels || length!=stride*i.height)return -1;
    for(size_t row=0;row<i.height;row++) {
        int64_t source=(int64_t)row-offset;
        if(source>=0 && source<height)memcpy(pixels+row*stride,packed+(size_t)source*stride,stride);
        else memset(pixels+row*stride,0,stride);
    }
    return 0;
}
wx_status_t cli_send(wx_device_t *d,const cli_canvas *c,int offset,int overlay)
{
    wx_surface_t *s=wx_device_surface(d);wx_surface_info_t i;
    if(wx_surface_info(s,&i)!=WX_OK || (size_t)c->width!=i.width || cli_frame(c,s,offset))return WX_INVALID;
    if(!overlay)return wx_device_bitmap(d);
    for(size_t y=0;y<i.height;y++) {
        int source=(int)y-offset;if(source<0 || source>=c->height)continue;
        const unsigned char *mask=c->mask+(size_t)source*(unsigned)c->width;
        size_t x=0;while(x<i.width) {
            while(x<i.width && !mask[x])x++;
            size_t start=x;while(x<i.width && mask[x])x++;
            if(x>start) {wx_status_t st=wx_device_bitmap_region(d,start,y,x-start,1);if(st!=WX_OK)return st;}
        }
    }
    return WX_OK;
}
int cli_scroll_y(int down,int screen,int content,double speed,double seconds)
{
    double distance=floor(seconds*speed);
    if(distance>screen+content)distance=screen+content;
    return down?-content+(int)distance:screen-(int)distance;
}
