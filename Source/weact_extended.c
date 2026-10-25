/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#define _POSIX_C_SOURCE 200809L
#include "weact_internal.h"
#include "weact_bitmap_internal.h"
#include <time.h>
#include "weact_protocol.h"
#include <string.h>

static bool geometry(wx_model_t model, uint8_t orientation, size_t *w, size_t *h)
{
    if (orientation > 3 || (model != WX_FS096 && model != WX_FS35)) return false;
    size_t narrow = model == WX_FS35 ? 320 : 80;
    size_t tall = model == WX_FS35 ? 480 : 160;
    *w = orientation < 2 ? narrow : tall;
    *h = orientation < 2 ? tall : narrow;
    return true;
}

wx_status_t wx_device_create(const wx_device_options_t *o, wx_device_t **out)
{
    if (!out) return WX_INVALID;
    *out = NULL;
    if (!o || o->struct_size != sizeof(*o) || !wx_allocator_valid(&o->allocator) ||
        !o->timeout_ms || !o->transport.write || !o->transport.now_ms ||
        !o->transport.wait_writable) return WX_INVALID;
    size_t w, h;
    if (!geometry(o->model, o->orientation, &w, &h) || w != o->width || h != o->height)
        return WX_INVALID;
    wx_pixel_format_t format = o->model == WX_FS35 ? WX_RGB565_LE : WX_LEGACY_BRG565_BE;
    wx_surface_t *surface = NULL;
    wx_status_t status = wx_surface_create(w, h, o->stride, format, &o->allocator, &surface);
    if (status != WX_OK) return status;
    wx_device_t *d = wx_allocate(&o->allocator, sizeof(*d));
    if (!d) {
        wx_surface_destroy(surface);
        return WX_NOMEM;
    }
    *d = (wx_device_t){o->model, o->orientation, surface, o->allocator,
                       o->transport, o->timeout_ms, false, NULL};
    *out = d;
    return WX_OK;
}
void wx_device_destroy(wx_device_t *d)
{
    if (!d) return;
    wx_allocator_t a = d->allocator;
    wx_init_metadata_destroy(d);
    wx_surface_destroy(d->surface);
    wx_release(&a, d);
}
wx_surface_t *wx_device_surface(wx_device_t *d)
{
    if (d && d->init) d->init->info.fb_sync = WX_FB_UNKNOWN;
    return d ? d->surface : NULL;
}
bool wx_device_stream_failed(const wx_device_t *d)
{
    return d && d->failed;
}

static uint64_t deadline(wx_device_t *d)
{
    uint64_t now = d->transport.now_ms(d->transport.context);
    return d->timeout_ms > UINT64_MAX - now ? UINT64_MAX : now + d->timeout_ms;
}
static wx_status_t fail(wx_device_t *d, wx_status_t status)
{
    d->failed = true;
    if (d->init) {
        d->init->report.status=d->init->report.primary_status=status;
        d->init->report.reason=WX_INIT_TRANSPORT_ERROR;
        d->init->info.last_reason=WX_INIT_TRANSPORT_ERROR;
        (void)wx_init_dispose_channel(d,true);
    }
    return status;
}
/* All pieces of one command share a single absolute deadline. No sleeps and
 * no retry of a header after a failed transaction. */
static uint64_t measure_ns(void)
{
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec : 0;
}
static wx_status_t send_bytes_measured(wx_device_t *d, const uint8_t *p, size_t length,
                              uint64_t end,wx_bitmap_stats_t *stats,bool pixels)
{
    while (length) {
        if (d->transport.now_ms(d->transport.context) >= end) return fail(d, WX_TIMEOUT);
        size_t written = 0;
        uint64_t start=stats && stats->measure_time?measure_ns():0;
        wx_status_t status = d->transport.write(d->transport.context, p, length, &written);
        if(stats) {
            if(stats->measure_time){uint64_t finish=measure_ns();if(start && finish>=start)stats->write_ns+=finish-start;}
            ++stats->write_calls;
            if(written<=length) {
                if(pixels)stats->pixel_bytes+=written;else stats->header_bytes+=written;
            }
        }
        if (written > length || (status != WX_OK && written)) return fail(d, WX_IO);
        if (d->transport.now_ms(d->transport.context) >= end) return fail(d, WX_TIMEOUT);
        if (status == WX_OK && written) {
            p += written;
            length -= written;
            continue;
        }
        if (status == WX_INTERRUPTED) continue;
        if (status != WX_OK && status != WX_AGAIN) return fail(d, status);
        start=stats && stats->measure_time?measure_ns():0;
        status = d->transport.wait_writable(d->transport.context, end);
        if(stats && stats->measure_time){uint64_t finish=measure_ns();if(start && finish>=start)stats->wait_ns+=finish-start;}
        if (status != WX_OK && status != WX_INTERRUPTED && status != WX_AGAIN)
            return fail(d, status);
    }
    return WX_OK;
}
static wx_status_t send_bytes(wx_device_t *d,const uint8_t *p,size_t length,uint64_t end)
{ return send_bytes_measured(d,p,length,end,NULL,false); }
static wx_status_t command(wx_device_t *d, const uint8_t *p, size_t length)
{
    if (d->failed) return WX_STREAM_FAILED;
    return send_bytes(d, p, length, deadline(d));
}
wx_status_t wx_device_set_brightness(wx_device_t *d, uint8_t value, uint16_t time_ms)
{
    if (!d) return WX_INVALID;
    if (d->init && d->failed) return WX_STREAM_FAILED;
    uint8_t p[5];
    wx_status_t status = wx_encode_brightness(p, sizeof(p), value, time_ms);
    return status == WX_OK ? command(d, p, sizeof(p)) : status;
}
wx_status_t wx_device_set_orientation(wx_device_t *d, uint8_t orientation)
{
    if (!d) return WX_INVALID;
    if (d->failed) return WX_STREAM_FAILED;
    if (d->init && orientation!=2) {
        d->init->info.last_reason=WX_INIT_PROFILE_NOT_ENABLED;
        return WX_INVALID;
    }
    size_t w, h;
    if (!geometry(d->model, orientation, &w, &h)) return WX_INVALID;
    /* Allocate before sending; allocation failure never mutates device state
     * or the stream. Keep the borrowed surface object stable across rotation. */
    wx_surface_t *replacement = NULL;
    wx_status_t status = wx_surface_create(w, h, 0, d->surface->info.format,
                                           &d->allocator, &replacement);
    if (status != WX_OK) return status;
    uint8_t p[3];
    status = wx_encode_orientation(p, sizeof(p), orientation);
    if (status == WX_OK) {
        if (d->init) {
            d->init->info.geometry_usable=false;
            d->init->info.confirmed_valid=false;
            d->init->info.confirmed_width=d->init->info.confirmed_height=0;
            d->init->info.fb_sync=WX_FB_UNKNOWN;
            d->init->info.requested_valid=true;
            d->init->info.requested_orientation=orientation;
            d->init->info.state=WX_INIT_CONNECTED;
        }
        status = command(d, p, sizeof(p));
    }
    if (status == WX_OK) {
        wx_surface_t old = *d->surface;
        *d->surface = *replacement;
        *replacement = old;
        d->orientation = orientation;
    }
    wx_surface_destroy(replacement);
    return status;
}
wx_status_t wx_device_fill(wx_device_t *d, uint16_t color)
{
    if (!d) return WX_INVALID;
    if (d->init) {
        if (d->failed) return WX_STREAM_FAILED;
        if (!d->init->info.geometry_usable) {
            d->init->info.last_reason = WX_INIT_GEOMETRY_UNCONFIRMED;
            return WX_INVALID;
        }
        d->init->info.fb_sync=WX_FB_UNKNOWN;
    }
    wx_surface_info_t *i = &d->surface->info;
    uint8_t p[12];
    wx_status_t status = wx_encode_fill(p, sizeof(p), 0, 0,
                                        (uint32_t)i->width - 1, (uint32_t)i->height - 1,
                                        color);
    return status == WX_OK ? command(d, p, sizeof(p)) : status;
}
wx_status_t wx_device_bitmap(wx_device_t *d)
{
    if (!d) return WX_INVALID;
    if (d->failed) return WX_STREAM_FAILED;
    if (d->init) {
        if (d->failed) return WX_STREAM_FAILED;
        if (!d->init->info.geometry_usable) {
            d->init->info.last_reason = WX_INIT_GEOMETRY_UNCONFIRMED;
            return WX_INVALID;
        }
        d->init->info.fb_sync=WX_FB_UNKNOWN;
    }
    wx_surface_info_t *i = &d->surface->info;
    uint8_t p[10];
    wx_status_t status = wx_encode_bitmap(p, sizeof(p), 0, 0,
                                          (uint32_t)i->width - 1, (uint32_t)i->height - 1);
    if (status != WX_OK) return status;
    uint64_t end = deadline(d);
    status = send_bytes(d, p, sizeof(p), end);
    for (size_t y = 0; status == WX_OK && y < i->height; ++y)
        status = send_bytes(d, d->surface->pixels + y * i->stride, i->width * 2, end);
    return status;
}

wx_status_t wx_device_bitmap_rect_internal(wx_device_t *d,size_t x,size_t y,
                                           size_t width,size_t height,wx_bitmap_stats_t *stats)
{
    if(stats){bool timing=stats->measure_time;*stats=(wx_bitmap_stats_t){.measure_time=timing};}
    if(!d)return WX_INVALID;
    if(d->failed)return WX_STREAM_FAILED;
    if(!d->init || d->model!=WX_FS35 || d->orientation!=2)return WX_INVALID;
    if(!d->init->info.geometry_usable || !d->init->info.confirmed_valid ||
       (d->init->info.confirmed_orientation!=2 && d->init->info.confirmed_orientation!=3) || d->init->info.confirmed_width!=480 ||
       d->init->info.confirmed_height!=320) {
        d->init->info.last_reason=WX_INIT_GEOMETRY_UNCONFIRMED;return WX_INVALID;
    }
    const wx_surface_info_t *i=&d->surface->info;
    if(i->width!=480 || i->height!=320 || i->format!=WX_RGB565_LE)return WX_INVALID;
    if(!width || !height || x>=i->width || y>=i->height ||
       width>i->width-x || height>i->height-y)return WX_BOUNDS;
    uint8_t header[10];
    wx_status_t status=wx_encode_bitmap(header,sizeof(header),(uint32_t)x,(uint32_t)y,
                                        (uint32_t)(x+width-1),(uint32_t)(y+height-1));
    if(status!=WX_OK)return status;
    d->init->info.fb_sync=WX_FB_UNKNOWN;
    uint64_t end=deadline(d);
    status=send_bytes_measured(d,header,sizeof(header),end,stats,false);
    for(size_t row=y;status==WX_OK && row<y+height;++row)
        status=send_bytes_measured(d,d->surface->pixels+row*i->stride+x*2,width*2,end,stats,true);
    return status;
}

#include "weact_fs35.h"
#include "weact_fastlz.h"
static bool fs35_ready(const wx_device_t *d)
{
    return d && d->init && d->init->owned && !d->init->sealed && d->model==WX_FS35 &&
        (d->init->info.state==WX_INIT_READY || d->init->info.state==WX_INIT_READY_PRESERVED);
}
wx_status_t wx_device_set_backlight_percent(wx_device_t *d,unsigned percent,uint16_t fade)
{
    if(!d)return WX_INVALID;
    if(d->failed)return WX_STREAM_FAILED;
    if(!fs35_ready(d) || percent>100)return WX_INVALID;
    return wx_device_set_brightness(d,(uint8_t)(percent*255u/100u),fade);
}
wx_status_t wx_device_get_backlight(wx_device_t *d,uint8_t *value)
{
    if(!d || !value)return WX_INVALID;
    if(d->failed)return WX_STREAM_FAILED;
    if(!fs35_ready(d))return WX_INVALID;
    wx_init_channel_t *c=&d->init->channel;void *ctx=c->tx.context;
    uint64_t end=deadline(d);uint8_t reply[3];size_t used=0;
    wx_status_t s=c->check_guards(ctx,WX_INIT_STAGE_QUERY_TX,end,&d->init->report);
    if(s==WX_OK)s=c->probe_quiet(ctx,end);
    const uint8_t query[]={0x83,0x0a};
    if(s==WX_OK)s=send_bytes(d,query,sizeof(query),end);
    /* send_bytes may release and invalidate the channel on error. */
    if(s!=WX_OK)return d->failed?s:fail(d,s);
    while(used<sizeof(reply)) {
        if(c->tx.now_ms(ctx)>=end)return fail(d,WX_TIMEOUT);
        /* Raw CDC/PTY has VMIN=0: an immediate empty read is not evidence of
         * EOF. Wait for readability first, just like orientation readback. */
        s=c->wait_readable(ctx,end);
        if(c->tx.now_ms(ctx)>=end)return fail(d,WX_TIMEOUT);
        if(s==WX_AGAIN || s==WX_INTERRUPTED)continue;
        if(s!=WX_OK)return fail(d,s);
        s=c->check_guards(ctx,WX_INIT_STAGE_RX,end,&d->init->report);
        if(s!=WX_OK)return fail(d,s);
        size_t got=0;s=c->read(ctx,reply+used,sizeof(reply)-used,&got);
        if(got>sizeof(reply)-used || (s!=WX_OK && got))return fail(d,WX_IO);
        if(c->tx.now_ms(ctx)>=end)return fail(d,WX_TIMEOUT);
        if(s==WX_OK && got){used+=got;continue;}
        if(s==WX_INTERRUPTED)continue;
        if(s!=WX_AGAIN)return fail(d,s==WX_OK?WX_IO:s);
    }
    if(reply[0]!=0x83 || reply[2]!=0x0a)return fail(d,WX_IO);
    s=c->probe_quiet(ctx,end);
    if(s==WX_OK)s=c->check_guards(ctx,WX_INIT_STAGE_COMMIT,end,&d->init->report);
    if(s!=WX_OK)return fail(d,s);
    if(c->tx.now_ms(ctx)>=end)return fail(d,WX_TIMEOUT);
    *value=reply[1];return WX_OK;
}
wx_status_t wx_device_bitmap_fastlz(wx_device_t *d,wx_fastlz_stats_t *stats)
{return wx_device_bitmap_fastlz_rect(d,0,0,480,320,stats);}
wx_status_t wx_device_bitmap_fastlz_rect(wx_device_t *d,size_t x,size_t y,size_t w,size_t h,wx_fastlz_stats_t *stats)
{
    if(stats) {
        if(stats->struct_size!=sizeof(*stats))return WX_INVALID;
        bool timing=stats->measure_time;*stats=(wx_fastlz_stats_t){.struct_size=sizeof(*stats),.measure_time=timing};
    }
    if(!d)return WX_INVALID;
    if(d->failed)return WX_STREAM_FAILED;
    if(!fs35_ready(d) || d->orientation!=2 || !d->init->info.geometry_usable ||
       !d->init->info.confirmed_valid || (d->init->info.confirmed_orientation!=2 && d->init->info.confirmed_orientation!=3) ||
       d->init->info.confirmed_width!=480 || d->init->info.confirmed_height!=320)return WX_INVALID;
    wx_surface_info_t *i=&d->surface->info;
    if(i->width!=480 || i->height!=320 || i->format!=WX_RGB565_LE)return WX_INVALID;
    if(!w || !h || x>=480 || y>=320 || w>480-x || h>320-y)return WX_BOUNDS;
    /* Validated fixed geometry bounds all arithmetic (at most 307200 bytes).
     * Buffer holds worst-case literal stream plus every four-byte prefix.
     * No allocations after the first byte is sent. */
    size_t total=w*h*2,blocks=(total+WX_FASTLZ_BLOCK_SIZE-1)/WX_FASTLZ_BLOCK_SIZE;
    size_t capacity=total+(total+31)/32+blocks*5;
    uint8_t *wire=wx_allocate(&d->allocator,capacity);
    if(!wire)return WX_NOMEM;
    bool timing=stats && stats->measure_time;
    uint64_t start=timing?measure_ns():0;
    uint8_t raw[WX_FASTLZ_BLOCK_SIZE];size_t used=0,done=0,compressed=0;
    wx_status_t status=WX_OK;
    while(done<total) {
        size_t n=total-done;if(n>sizeof(raw))n=sizeof(raw);
        for(size_t copied=0;copied<n;) {
            size_t at=done+copied,row=at/(w*2),column=at%(w*2);
            size_t count=w*2-column;if(count>n-copied)count=n-copied;
            memcpy(raw+copied,d->surface->pixels+(y+row)*i->stride+x*2+column,count);
            copied+=count;
        }
        size_t size=0;
        status=wx_fastlz_encode(raw,n,wire+used+4,capacity-used-4,&size);
        if(status!=WX_OK)break;
        wire[used]=(uint8_t)n;wire[used+1]=(uint8_t)(n>>8);
        wire[used+2]=(uint8_t)size;wire[used+3]=(uint8_t)(size>>8);
        used+=4+size;compressed+=size;done+=n;
    }
    if(stats){stats->original_bytes=total;stats->compressed_bytes=compressed;stats->block_bytes=used;
        if(timing && start)stats->compression_ns=measure_ns()-start;}
    if(status==WX_OK) {
        uint8_t header[10];status=wx_encode_bitmap_fastlz(header,sizeof(header),(uint32_t)x,(uint32_t)y,(uint32_t)(x+w-1),(uint32_t)(y+h-1));
        wx_bitmap_stats_t tx={.measure_time=timing};
        uint64_t end=deadline(d);start=timing?measure_ns():0;
        d->init->info.fb_sync=WX_FB_UNKNOWN;
        if(status==WX_OK)status=send_bytes_measured(d,header,sizeof(header),end,&tx,false);
        for(size_t offset=0;status==WX_OK && offset<used;) {
            size_t size=(size_t)wire[offset+2]+((size_t)wire[offset+3]<<8)+4;
            status=send_bytes_measured(d,wire+offset,size,end,&tx,true);
            if(status==WX_OK && stats)++stats->blocks;
            offset+=size;
        }
        if(stats){stats->wire_bytes=tx.header_bytes+tx.pixel_bytes;stats->wait_ns=tx.wait_ns;stats->write_ns=tx.write_ns;
            if(timing && start)stats->tx_ns=measure_ns()-start;}
    }
    wx_release(&d->allocator,wire);return status;
}

/* Additive Stage 7.5 public region sender; reuse the existing transaction path. */
wx_status_t wx_device_bitmap_region(wx_device_t *d,size_t x,size_t y,size_t w,size_t h)
{
    if(!d)return WX_INVALID;
    if(d->failed)return WX_STREAM_FAILED;
    if(d->init && !d->init->info.geometry_usable)return WX_INVALID;
    const wx_surface_info_t *i=&d->surface->info;
    if(!w || !h || x>=i->width || y>=i->height || w>i->width-x || h>i->height-y)
        return WX_BOUNDS;
    uint8_t header[10];
    wx_status_t st=wx_encode_bitmap(header,sizeof(header),(uint32_t)x,(uint32_t)y,
                                   (uint32_t)(x+w-1),(uint32_t)(y+h-1));
    if(st!=WX_OK)return st;
    if(d->init)d->init->info.fb_sync=WX_FB_UNKNOWN;
    uint64_t end=deadline(d);
    st=send_bytes(d,header,sizeof(header),end);
    for(size_t row=y;st==WX_OK && row<y+h;row++)
        st=send_bytes(d,d->surface->pixels+row*i->stride+x*2,w*2,end);
    return st;
}
