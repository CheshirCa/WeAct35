/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Linux WeActCLI adapted from CheshirCa's PureBasic CLI with permission.
 * SPDX-License-Identifier: GPL-3.0-only. See /usr/share/common-licenses/GPL-3.
 */
#define _POSIX_C_SOURCE 200809L
#include "weact_cli.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
volatile sig_atomic_t cli_stopped;
void cli_signal_stop(int signal_number){cli_stopped=signal_number;}
static double seconds(void)
{struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+t.tv_nsec/1e9;}
#include "weact_fs35.h"
#include "weact_init.h"
#include "weact_bitmap_internal.h"
#include <errno.h>
#include <math.h>
/* Bounded histograms: bucket upper bounds .25, .5, 1, ... 8192 ms;
 * final bucket is unbounded. These are bounds, not exact percentiles. */
static void sample(cli_latency *l,double value)
{
    if(value<0)value=0;
    l->count++;l->sum+=value;if(value>l->max)l->max=value;
    size_t b=0;double bound=0.00025;
    while(b<15 && value>bound){b++;bound*=2;}
    l->buckets[b]++;
}
static double percentile(const cli_latency *l,unsigned percent)
{
    if(!l->count)return 0;
    size_t target=(l->count*percent+99)/100,count=0;
    double bound=0.25;
    for(size_t b=0;b<16;b++,bound*=2) {
        count+=l->buckets[b];if(count>=target)return b==15?l->max*1000:bound;
    }
    return l->max*1000;
}
static void latency(const char *name,const cli_latency *l)
{
    fprintf(stderr," scroll_%s_n=%zu scroll_%s_mean_ms=%.3f scroll_%s_p50_bound_ms=%.3f scroll_%s_p95_bound_ms=%.3f scroll_%s_max_ms=%.3f",
            name,l->count,name,l->count?l->sum*1000/l->count:0,
            name,percentile(l,50),name,percentile(l,95),name,l->max*1000);
}
typedef struct {size_t x,y,w,h;} region;
static size_t cost(region r){return 10+2*r.w*r.h;}
static int send_region(wx_device_t *d,region r,int full,cli_scroll_metrics *m)
{
    double start=seconds();wx_status_t st;
    wx_fastlz_stats_t stats={.struct_size=sizeof(stats),.measure_time=true};
    wx_bitmap_stats_t raw={.measure_time=true};wx_init_info_t init={.struct_size=sizeof(init)};
    int measured_raw=!m->compression && wx_device_get_init_info(d,&init)==WX_OK && init.managed && init.model==WX_FS35;
    if(m->compression)st=wx_device_bitmap_fastlz_rect(d,r.x,r.y,r.w,r.h,&stats);
    else if(measured_raw)st=wx_device_bitmap_rect_internal(d,r.x,r.y,r.w,r.h,&raw);
    else st=full?wx_device_bitmap(d):wx_device_bitmap_region(d,r.x,r.y,r.w,r.h);
    double duration=seconds()-start;sample(&m->bitmap,duration);m->bitmap_seconds+=duration;
    if(measured_raw){stats.write_ns=raw.write_ns;stats.wait_ns=raw.wait_ns;stats.tx_ns=(uint64_t)(duration*1e9);}
    if(m->compression || measured_raw) {
        m->io_measured=1;sample(&m->tx,stats.tx_ns/1e9);sample(&m->write,stats.write_ns/1e9);sample(&m->poll,stats.wait_ns/1e9);
    }
    if(m->compression)sample(&m->encode,stats.compression_ns/1e9);
    m->compression_seconds+=stats.compression_ns/1e9;m->tx_seconds+=stats.tx_ns/1e9;
    m->write_seconds+=stats.write_ns/1e9;m->wait_seconds+=stats.wait_ns/1e9;
    if(st!=WX_OK)return -1;
    m->wire_bytes+=m->compression?stats.wire_bytes:cost(r);m->regions++;
    if(full)m->full_frames++;
    return 0;
}
int cli_scroll_update(wx_device_t *d,unsigned char *previous,int first,cli_scroll_metrics *m)
{
    wx_surface_t *surface=wx_device_surface(d);wx_surface_info_t info;
    if(!previous || !m || wx_surface_info(surface,&info)!=WX_OK ||
       !info.width || info.width>480 || !info.height || info.height>320)return -1;
    size_t bytes=info.width*info.height*2,length;
    unsigned char *pixels=wx_surface_pixels(surface,&length);
    if(!pixels || length!=bytes || info.stride!=info.width*2)return -1;
    if(m->compression && info.format!=WX_RGB565_LE)return -1;
    double start=seconds();region bands[40];size_t count=0,total=0;
    for(size_t y=0;y<info.height;y+=8) {
        size_t end=y+8<info.height?y+8:info.height,left=info.width,right=0;
        for(size_t row=y;row<end;row++)for(size_t x=0;x<info.width;x++) {
            size_t n=(row*info.width+x)*2;
            if(pixels[n]!=previous[n] || pixels[n+1]!=previous[n+1]) {
                if(x<left)left=x;
                if(x+1>right)right=x+1;
            }
        }
        if(right>left) {
            region r={left,y,right-left,end-y};
            if(count) {
                region old=bands[count-1];size_t x=old.x<r.x?old.x:r.x;
                size_t right_edge=old.x+old.w>r.x+r.w?old.x+old.w:r.x+r.w;
                region merged={x,old.y,right_edge-x,end-old.y};
                /* Raw: merge only when total bytes do not increase. FastLZ:
                 * one bounding region compresses the blank gaps and reduces
                 * synchronous transactions; no speculative raw retry. */
                if(m->compression || cost(merged)<=cost(old)+cost(r)) {
                    total-=cost(old);bands[count-1]=merged;total+=cost(merged);continue;
                }
            }
            bands[count++]=r;total+=cost(r);
        }
    }
    sample(&m->plan,seconds()-start);
    if(!first && !count)return 0;
    if(first || total>=bytes+10) {
        if(send_region(d,(region){0,0,info.width,info.height},1,m))return -1;
    } else for(size_t n=0;n<count;n++) {
        if(cli_stopped)return 1;
        if(send_region(d,bands[n],0,m))return -1;
    }
    if(cli_stopped)return 1;
    m->frames++;m->full_wire_bytes+=bytes+10;
    start=seconds();memcpy(previous,pixels,bytes);sample(&m->commit,seconds()-start);
    return 0;
}
int cli_present(wx_device_t *d,const cli_options *o,const cli_canvas *c,int realtime)
{
    if(o->code_format)return cli_stopped?0:
        (o->code_region?cli_code_region_send(d,o,c):cli_code_send(d,c))==WX_OK?0:-1;
    wx_surface_info_t info;if(wx_surface_info(wx_device_surface(d),&info)!=WX_OK)return -1;
    if(!o->scroll || o->cls || *o->image) {
        int offset=*o->image || o->cls?0:o->y>=0?o->y:((int)info.height-c->height)/2;
        if(!*o->image && !o->cls && o->y<0 && offset<5)offset=5;
        return cli_send(d,c,offset,o->overlay && !o->cls)==WX_OK?0:-1;
    }
    if(!(o->speed>0) || !isfinite(o->speed) || !isfinite(1/o->speed) ||
       info.width>480 || info.height>320 || info.stride!=info.width*2 ||
       c->width!=(int)info.width || c->height<=0 ||
       (o->scroll_compression && info.format!=WX_RGB565_LE))return -1;
    double start=seconds();if(start<0)return -1;
    size_t bytes=info.width*info.height*2;
    unsigned char *previous=calloc(1,bytes);if(!previous)return -1;
    wx_surface_t *packed=NULL;
    if(wx_surface_create(info.width,(size_t)c->height,0,info.format,NULL,&packed)!=WX_OK) {free(previous);return -1;}
    if(wx_surface_bitmap_rgb(packed,0,0,info.width,(size_t)c->height,c->rgb,(size_t)c->width*(unsigned)c->height*3,0)!=WX_OK) {
        wx_surface_destroy(packed);free(previous);return -1;
    }
    double pack_time=seconds()-start;size_t packed_length;
    const unsigned char *pixels=wx_surface_pixels(packed,&packed_length);(void)packed_length;
    int result=0,max_step=0,last=INT_MIN;size_t overruns=0;
    cli_scroll_metrics metrics={.compression=o->scroll_compression};
    double period=1/o->speed,next=seconds(),motion_start=next,last_complete=0;
    int distance=0,end=(int)info.height+c->height;
    for(;;) {
        if(cli_stopped)break;
        double now=seconds();if(now<0){result=-1;break;}
        double wait_start=now;
        while(realtime && distance && now<next && !cli_stopped && !result) {
            double remaining=next-now;if(remaining>0.033)remaining=0.033;
            struct timespec delay={.tv_nsec=(long)ceil(remaining*1e9)};
            if(nanosleep(&delay,NULL)<0 && errno!=EINTR)result=-1;
            now=seconds();if(now<0)result=-1;
        }
        sample(&metrics.wait,seconds()-wait_start);
        if(cli_stopped || result)break;
        double frame_start=seconds(),coord=frame_start;
        int y=o->down?-c->height+distance:(int)info.height-distance;
        if(last!=INT_MIN && abs(y-last)>max_step)max_step=abs(y-last);
        sample(&metrics.coordinate,seconds()-coord);
        double raster_start=seconds();
        if(cli_packed_frame(pixels,info.width*2,c->height,wx_device_surface(d),y)){result=-1;break;}
        double raster=seconds()-raster_start;metrics.raster_seconds+=raster;sample(&metrics.prepare,raster);
        double transfer_start=seconds();
        if(cli_scroll_update(d,previous,distance==0,&metrics)){result=cli_stopped?0:-1;break;}
        metrics.transfer_seconds+=seconds()-transfer_start;
        double complete=seconds();sample(&metrics.frame,complete-frame_start);
        if(last_complete)sample(&metrics.interval,complete-last_complete);
        last_complete=complete;last=y;
        if(!distance) {
            /* The initial full black transfer precedes motion. Never convert
             * its latency into a jump across the first visible glyph rows. */
            motion_start=complete;next=complete+period;
        } else {
            next+=period;
            if(complete>next){overruns++;next=complete;}
        }
        if(distance==end)break;
        distance++;
    }
    if(o->scroll_stats) {
        double duration=seconds()-start,motion=seconds()-motion_start;
        fprintf(stderr,"scroll_frames=%zu scroll_regions=%zu scroll_full_frames=%zu "
                "scroll_wire_bytes=%zu scroll_full_wire_bytes=%zu "
                "scroll_raster_ms=%.3f scroll_update_ms=%.3f scroll_bitmap_ms=%.3f scroll_elapsed_ms=%.3f "
                "scroll_fps=%.3f scroll_max_step=%d scroll_requested_px_s=%.3f scroll_actual_px_s=%.3f "
                "scroll_overruns=%zu scroll_pack_ms=%.3f scroll_compression=%s scroll_lz_ms=%.3f scroll_tx_ms=%.3f scroll_write_ms=%.3f scroll_poll_ms=%.3f",
                metrics.frames,metrics.regions,metrics.full_frames,metrics.wire_bytes,metrics.full_wire_bytes,
                metrics.raster_seconds*1000,metrics.transfer_seconds*1000,metrics.bitmap_seconds*1000,duration*1000,
                duration>0?metrics.frames/duration:0,max_step,o->speed,motion>0 && last!=INT_MIN?abs(last-(o->down?-c->height:(int)info.height))/motion:0,
                overruns,pack_time*1000,o->scroll_compression?"fastlz":"off",metrics.compression_seconds*1000,
                metrics.io_measured?metrics.tx_seconds*1000:-1,
                metrics.io_measured?metrics.write_seconds*1000:-1,metrics.io_measured?metrics.wait_seconds*1000:-1);
        latency("coord",&metrics.coordinate);latency("prepare",&metrics.prepare);
        latency("plan",&metrics.plan);latency("bitmap",&metrics.bitmap);
        latency("commit",&metrics.commit);latency("wait",&metrics.wait);
        latency("frame",&metrics.frame);latency("interval",&metrics.interval);
        latency("encode",&metrics.encode);latency("tx",&metrics.tx);latency("write",&metrics.write);latency("poll",&metrics.poll);fputc('\n',stderr);
    }
    wx_surface_destroy(packed);free(previous);
    if(result==0 && !cli_stopped && realtime) {struct timespec delay={.tv_nsec=300000000};nanosleep(&delay,NULL);}
    return result;
}
#ifndef WEACT_CLI_TEST
int main(int argc,char **argv)
{
    cli_options o;cli_canvas c={0};wx_device_t *d=NULL;wx_code_t *code=NULL;int fd=-1,result=1;
    if(cli_parse(argc,argv,&o)) {fprintf(stderr,"%s. Use weact-cli /?\n",o.argument_error?o.argument_error:"Invalid arguments");cli_options_free(&o);return 2;}
    if(o.help){cli_help();cli_options_free(&o);return 0;}
    if(o.code_format && cli_code_encode(&o,&code))goto done;
    if(cli_input(&o,STDIN_FILENO)) {fputs("Input missing, invalid, too large or timed out\n",stderr);goto done;}
    int model=cli_identify(&o);
    if(!model){fputs("Selected node is not a verified FS35/FS096 USB display\n",stderr);goto done;}
    if(o.init && model!=WX_FS35) {fputs("/init is supported only for FS35\n",stderr);goto done;}
    if(o.init) {
        struct sigaction action={.sa_handler=cli_signal_stop};sigemptyset(&action.sa_mask);
        sigaction(SIGINT,&action,NULL);sigaction(SIGTERM,&action,NULL);sigaction(SIGHUP,&action,NULL);
        if(cli_connect(&o,model,&d,&fd)){fputs("Guarded connection failed; no fallback\n",stderr);goto done;}
        result=cli_stopped?128+cli_stopped:0;goto done;
    }
    int w=model==WX_FS35?480:160,h=model==WX_FS35?320:80;
    if(o.scroll_compression && (model!=WX_FS35 || !o.scroll || o.overlay || o.cls || *o.image)) {
        fputs("Scroll FastLZ requires FS35 scrolling text\n",stderr);goto done;
    }
    if(!code && (o.x>=w || o.y>=h) && !o.cls){fputs("Text coordinates outside selected display\n",stderr);goto done;}
    double canvas_start=seconds();
    if(code) {if(cli_code_prepare(&o,code,model,&c))goto done;}
    else if(cli_canvas_make(&o,w,h,&c)){fputs("Could not render content (UTF-8, font, image or limits)\n",stderr);goto done;}
    if(o.scroll_stats && o.scroll)fprintf(stderr,"scroll_canvas_ms=%.3f\n",(seconds()-canvas_start)*1000);
    struct sigaction action={.sa_handler=cli_signal_stop};sigemptyset(&action.sa_mask);
    sigaction(SIGINT,&action,NULL);sigaction(SIGTERM,&action,NULL);sigaction(SIGHUP,&action,NULL);
    if(cli_connect(&o,model,&d,&fd)){fputs("Guarded connection failed; no fallback\n",stderr);goto done;}
    if(o.verbose)fprintf(stderr,"model=%s canvas=%dx%d overlay=%d visual_confirmed=0\n",model==WX_FS35?"FS35":"FS096",w,h,o.overlay);
    if(cli_present(d,&o,&c,1)){fputs("Transmission failed; stopping without retry\n",stderr);goto done;}
    result=cli_stopped?128+cli_stopped:0;
done:
    if(cli_disconnect(d,fd))result=1;
    wx_code_destroy(code);cli_canvas_free(&c);cli_options_free(&o);return result;
}
#endif
