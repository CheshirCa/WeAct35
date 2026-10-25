/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 * Existing USB worker: compare against last successful pixels, coalesce pending.
 */
#include "weactterm_presenter.h"
#include "weactterm_screen.h"
#include "weact_bitmap_internal.h"
#include "weact_init.h"
#include "weact_fs35.h"
#include <stdlib.h>
#include <string.h>
typedef struct {size_t x,y,w,h;} region;
static bool changed(const uint8_t *pixels,size_t stride,const uint8_t *last,int col,int row)
{
    size_t x=(size_t)col*8*2,y=(size_t)row*16;
    for(size_t line=y;line<y+16;++line)
        if(memcmp(pixels+line*stride+x,last+line*960+x,16))return true;
    return false;
}
/* At most 30 disjoint tile runs per row, 20 rows. No region-count fallback. */
#define MAX_REGIONS 600
typedef enum {FULL_NONE,FULL_BASELINE,FULL_BORROWED,FULL_COST} full_reason;
static size_t cost(region r) {return r.w*r.h*2+10;}
static region bounds(region a,region b)
{
    size_t x=a.x<b.x?a.x:b.x,y=a.y<b.y?a.y:b.y;
    size_t right=a.x+a.w>b.x+b.w?a.x+a.w:b.x+b.w;
    size_t bottom=a.y+a.h>b.y+b.h?a.y+a.h:b.y+b.h;
    return (region){x,y,right-x,bottom-y};
}
static region trim(const uint8_t *pixels,size_t stride,const uint8_t *last,region r)
{
    size_t left=r.x+r.w,top=r.y+r.h,right=r.x,bottom=r.y;
    for(size_t y=r.y;y<r.y+r.h;++y)for(size_t x=r.x;x<r.x+r.w;++x) {
        if(!memcmp(pixels+y*stride+x*2,last+y*960+x*2,2))continue;
        if(x<left)left=x;
        if(y<top)top=y;
        if(x+1>right)right=x+1;
        if(y+1>bottom)bottom=y+1;
    }
    return (region){left,top,right-left,bottom-top};
}
static size_t plan(wt_presenter *p,const uint8_t *pixels,size_t stride,
                   region out[MAX_REGIONS],full_reason *reason,size_t *merges)
{
    *reason=FULL_NONE;*merges=0;
    if(!p->baseline_valid){*reason=FULL_BASELINE;goto full;}
    size_t count=0;
    for(int row=0;row<20;++row)for(int col=0;col<60;) {
        if(!changed(pixels,stride,p->last,col,row)){++col;continue;}
        int first=col++;
        while(col<60 && changed(pixels,stride,p->last,col,row))++col;
        region r=trim(pixels,stride,p->last,
            (region){(size_t)first*8,(size_t)row*16,(size_t)(col-first)*8,16});
        /* Collapse exact vertical neighbours during collection, including
         * full screen changes, before the bounded cost merge pass. */
        bool merged=false;
        for(size_t n=0;n<count;++n)
            if(out[n].x==r.x && out[n].w==r.w && out[n].y+out[n].h==r.y) {
                out[n].h+=r.h;merged=true;++*merges;break;
            }
        if(!merged)out[count++]=r;
    }
    if(!count)return 0;
    if(!p->partial){*reason=FULL_BORROWED;goto full;}
    /* Actual wire cost: RGB565 bytes plus the existing 10-byte header.
     * No invented USB/LCD latency constant. Every merge strictly saves bytes.
     * Four bounded passes avoid an unbounded greedy search on fragmented UI. */
    for(unsigned pass=0;pass<4;++pass) {
        bool merged=false;
        for(size_t a=0;a<count;++a)for(size_t b=a+1;b<count;) {
            region r=bounds(out[a],out[b]);
            if(cost(r)<cost(out[a])+cost(out[b])) {
                out[a]=r;out[b]=out[--count];merged=true;++*merges;
            } else ++b;
        }
        if(!merged)break;
    }
    size_t bytes=0;
    for(size_t n=0;n<count;++n)bytes+=cost(out[n]);
    if(bytes>=307210){*reason=FULL_COST;goto full;}
    return count;
full:out[0]=(region){0,0,480,320};return 1;
}
static void *worker(void *ctx)
{
    wt_presenter *p=ctx;
    wx_surface_t *surface=wx_device_surface(p->device);wx_surface_info_t info;
    if(wx_surface_info(surface,&info)!=WX_OK)return NULL; /* Validated by open. */
    uint8_t *pixels=wx_surface_pixels(surface,NULL);
    pthread_mutex_lock(&p->lock);
    for(;;) {
        while(!p->available && !p->stop)pthread_cond_wait(&p->ready,&p->lock);
        if(!p->available)break;
        uint64_t start=p->perf_enabled?wt_now_ns():0;
        if(p->perf_enabled && p->pending_since && start>=p->pending_since)p->queue_wait_ns+=start-p->pending_since;
        for(size_t y=0;y<320;++y)memcpy(pixels+y*info.stride,p->pending+y*960,960);
        if(p->perf_enabled)p->copy_ns+=wt_elapsed_ns(start);
        p->available=false;
        pthread_mutex_unlock(&p->lock);
        region regions[MAX_REGIONS];full_reason reason;size_t merges;
        start=p->perf_enabled?wt_now_ns():0;
        size_t count=plan(p,pixels,info.stride,regions,&reason,&merges);
        uint64_t diff_ns=p->perf_enabled?wt_elapsed_ns(start):0;
        wx_status_t status=WX_OK;
        size_t payload=0,headers=0,completed=0;
        uint64_t bitmap_ns=0,wait_ns=0,write_ns=0;
        for(size_t n=0;n<count && status==WX_OK;++n) {
            start=p->perf_enabled?wt_now_ns():0;
            if(p->fastlz) {
                wx_fastlz_stats_t stats={.struct_size=sizeof(stats),.measure_time=p->perf_enabled};
                status=wx_device_bitmap_fastlz_rect(p->device,regions[n].x,regions[n].y,regions[n].w,regions[n].h,&stats);
                p->original_bytes+=stats.original_bytes;p->compressed_bytes+=stats.compressed_bytes;
                p->compressed_block_bytes+=stats.block_bytes;p->wire_bytes+=stats.wire_bytes;
                p->compression_ns+=stats.compression_ns;p->transfer_ns+=stats.tx_ns;
                payload+=stats.wire_bytes>10?stats.wire_bytes-10:0;headers+=stats.wire_bytes<10?stats.wire_bytes:10;
                wait_ns+=stats.wait_ns;write_ns+=stats.write_ns;
                if(status==WX_OK)++p->compressed_regions;
            } else if(p->partial) {
                wx_bitmap_stats_t stats={.measure_time=p->perf_enabled};
                status=wx_device_bitmap_rect_internal(p->device,regions[n].x,regions[n].y,regions[n].w,regions[n].h,&stats);
                payload+=stats.pixel_bytes;headers+=stats.header_bytes;
                wait_ns+=stats.wait_ns;write_ns+=stats.write_ns;
            } else {
                status=wx_device_bitmap(p->device);
                if(status==WX_OK){payload+=307200;headers+=10;}
            }
            if(!p->fastlz) {
                p->original_bytes+=regions[n].w*regions[n].h*2;
            }
            if(p->perf_enabled)bitmap_ns+=wt_elapsed_ns(start);
            if(status==WX_OK)++completed;
        }
        uint64_t commit_ns=0;
        if(count && status==WX_OK) {
            start=p->perf_enabled?wt_now_ns():0;
            for(size_t y=0;y<320;++y)memcpy(p->last+y*960,pixels+y*info.stride,960);
            p->baseline_valid=true;
            if(p->perf_enabled)commit_ns=wt_elapsed_ns(start);
        }
        pthread_mutex_lock(&p->lock);
        p->status=status;++p->processed;if(count)++p->attempted;
        p->pixel_bytes+=payload;p->header_bytes+=headers;p->regions+=completed;
        if(!p->fastlz){p->wire_bytes+=payload+headers;p->transfer_ns+=bitmap_ns;}
        p->bitmap_ns+=bitmap_ns;p->tx_wait_ns+=wait_ns;p->tx_write_ns+=write_ns;
        p->copy_ns+=commit_ns;p->diff_ns+=diff_ns;p->region_merges+=merges;
        if(status==WX_OK && count) {
            ++p->sent;if(p->fastlz)++p->compressed_frames;
            if(count==1 && regions[0].w==480 && regions[0].h==320) {
                ++p->full_frames;
                if(reason==FULL_BASELINE)++p->full_baseline;
                else if(reason==FULL_BORROWED)++p->full_borrowed;
                else ++p->full_cost;
            }
        } else if(!count)++p->skipped;
        pthread_cond_broadcast(&p->ready);
        if(status!=WX_OK){p->available=false;break;}
    }
    pthread_mutex_unlock(&p->lock);return NULL;
}
int wt_presenter_open(wt_presenter *p,wx_device_t *device,bool perf)
{return wt_presenter_open_mode(p,device,perf,false);}
int wt_presenter_open_mode(wt_presenter *p,wx_device_t *device,bool perf,bool fastlz)
{
    memset(p,0,sizeof(*p));p->device=device;p->perf_enabled=perf;p->fastlz=fastlz;
    wx_surface_info_t info;wx_init_info_t init={.struct_size=sizeof(init)};
    if(wx_surface_info(wx_device_surface(device),&info)!=WX_OK || info.width!=480 || info.height!=320 || info.format!=WX_RGB565_LE ||
       wx_device_get_init_info(device,&init)!=WX_OK)return -1;
    p->partial=init.managed;
    if(fastlz && (!init.managed || !init.geometry_usable))return -1;
    p->pending=malloc(307200);p->last=malloc(307200);if(!p->pending || !p->last)goto failed;
    if(pthread_mutex_init(&p->lock,NULL))goto failed;
    if(pthread_cond_init(&p->ready,NULL))goto mutex_failed;
    if(pthread_create(&p->thread,NULL,worker,p))goto cond_failed;
    p->running=true;return 0;
cond_failed:pthread_cond_destroy(&p->ready);
mutex_failed:pthread_mutex_destroy(&p->lock);
failed:free(p->pending);free(p->last);p->pending=p->last=NULL;return -1;
}
wx_status_t wt_presenter_status(wt_presenter *p)
{
    pthread_mutex_lock(&p->lock);wx_status_t status=p->status;pthread_mutex_unlock(&p->lock);return status;
}
int wt_presenter_submit(wt_presenter *p,wx_surface_t *surface)
{
    wx_surface_info_t info;
    if(wx_surface_info(surface,&info)!=WX_OK || info.width!=480 || info.height!=320 || info.format!=WX_RGB565_LE)return -1;
    uint8_t *pixels=wx_surface_pixels(surface,NULL);
    pthread_mutex_lock(&p->lock);
    if(p->status!=WX_OK || p->stop){pthread_mutex_unlock(&p->lock);return -1;}
    uint64_t start=p->perf_enabled?wt_now_ns():0;
    for(size_t y=0;y<320;++y)memcpy(p->pending+y*960,pixels+y*info.stride,960);
    if(p->perf_enabled){p->pending_since=wt_now_ns();if(start && p->pending_since>=start)p->copy_ns+=p->pending_since-start;}
    p->available=true;pthread_cond_signal(&p->ready);pthread_mutex_unlock(&p->lock);return 0;
}
wx_status_t wt_presenter_close(wt_presenter *p)
{
    if(!p->running)return WX_OK;
    pthread_mutex_lock(&p->lock);p->stop=true;pthread_cond_signal(&p->ready);pthread_mutex_unlock(&p->lock);
    pthread_join(p->thread,NULL);p->running=false;
    pthread_cond_destroy(&p->ready);pthread_mutex_destroy(&p->lock);free(p->pending);free(p->last);p->pending=p->last=NULL;
    return p->status;
}
