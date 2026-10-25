/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Linux adaptation of WeActCLI.pb by CheshirCa, with author's permission.
 * CLI application: SPDX-License-Identifier: GPL-3.0-only. Library remains MIT.
 */
#ifndef WEACT_CLI_H
#define WEACT_CLI_H
#include "weact_graphics.h"
#include "weact_codes.h"
#include <signal.h>
#define CLI_TEXT_MAX 262144u
#define CLI_IMAGE_MAX (16u*1024u*1024u)
#define CLI_PIXELS_MAX (4096u*4096u)
typedef struct {
    char port[4096],serial[256],font[4096],file[4096],image[4096];
    char *text;int size,x,y,mode,iw,ih,quality;
    unsigned char color[3];double speed;
    int down,scroll,center,cls,verbose,allow_unknown,overlay,help,scroll_stats,scroll_compression,init;
    wx_code_format_t code_format; /* 0=text/image; application-local, not library ABI */
    char *code_data;
    int code_conflict;
    int code_region,width,height; /* app-only x/y/w/h; never public ABI */
    const char *argument_error;
} cli_options;
typedef struct {unsigned char *rgb,*mask;int width,height;} cli_canvas;
int cli_parse(int argc,char **argv,cli_options *o);
void cli_options_free(cli_options *o);
int cli_regular_open(const char *,size_t,size_t *);
int cli_input(cli_options *o,int fd);
int cli_canvas_make(cli_options *o,int width,int height,cli_canvas *out);
void cli_canvas_free(cli_canvas *c);
int cli_frame(const cli_canvas *c,wx_surface_t *s,int offset);
wx_status_t cli_send(wx_device_t *d,const cli_canvas *frame,int offset,int overlay);
int cli_packed_frame(const unsigned char *,size_t,int,wx_surface_t *,int);
int cli_scroll_y(int down,int screen,int content,double speed,double seconds);
extern volatile sig_atomic_t cli_stopped;
typedef struct {size_t count,buckets[16];double sum,max;} cli_latency;
typedef struct {
    size_t frames,regions,full_frames,wire_bytes,full_wire_bytes;
    double raster_seconds,transfer_seconds,bitmap_seconds;
    int compression,io_measured;
    cli_latency plan,bitmap,commit,prepare,coordinate,wait,frame,interval,encode,tx,write,poll;
    double compression_seconds,tx_seconds,write_seconds,wait_seconds;
} cli_scroll_metrics;
/* App-local updater: packed surface, previous frame updated only on success. */
int cli_scroll_update(wx_device_t *,unsigned char *,int,cli_scroll_metrics *);
int cli_present(wx_device_t *d,const cli_options *o,const cli_canvas *c,int realtime);
int cli_model_product(const char *vid,const char *pid,const char *product);
/* Application transport adapter; all display framing remains in libweact. */
int cli_connect(const cli_options *o,int model,wx_device_t **out,int *fd);
int cli_identify(const cli_options *o);
int cli_disconnect(wx_device_t *d,int fd);
void cli_help(void);
int cli_code_encode(const cli_options *,wx_code_t **);
int cli_code_canvas(const wx_code_t *,int,cli_canvas *);
int cli_code_prepare(const cli_options *,const wx_code_t *,int,cli_canvas *);
wx_status_t cli_code_region_send(wx_device_t *,const cli_options *,const cli_canvas *);
wx_status_t cli_code_send(wx_device_t *,const cli_canvas *);
#endif
