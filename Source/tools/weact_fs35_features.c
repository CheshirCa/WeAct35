/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * See ../LICENSE (MIT). Independent one-shot opt-in actions, no retries.
 */
#define _POSIX_C_SOURCE 200809L
#include "weact_fs35.h"
#include "weact_posix_init.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
static int execute(wx_init_options_t *options,bool bitmap)
{
    wx_device_t *d=NULL;wx_init_report_t r={.struct_size=sizeof(r)};
    wx_status_t status=wx_device_init_preserve(options,&d,&r);
    printf("preserve_status=%d reason=%d errno=%d unknown_accepted=%d\n",status,r.reason,r.primary_errno,r.unknown_accepted);
    if(status==WX_OK && bitmap) {
        wx_surface_t *s=wx_device_surface(d);
        /* 64x48 coloured quadrants, one moderate rectangle; canvas only. */
        for(size_t y=16;y<64;++y)for(size_t x=16;x<80;++x) {
            uint16_t color=y<40?(x<48?0xf800:0x07e0):(x<48?0x001f:0xffff);
            status=wx_surface_set_pixel(s,x,y,color);if(status!=WX_OK)break;
        }
        /* The last protocol operation before 15 is this required query. No
         * orientation setter, Fill, reset, brightness or automatic retry. */
        if(status==WX_OK)status=wx_device_confirm_orientation(d,&r);
        printf("confirm_status=%d matching=%d RX=%zu\n",status,r.orientation_matching,r.rx_length);
        wx_fastlz_stats_t stats={.struct_size=sizeof(stats),.measure_time=true};
        if(status==WX_OK)status=wx_device_bitmap_fastlz_rect(d,16,16,64,48,&stats);
        printf("fastlz_status=%d original=%zu blocks=%zu compressed=%zu wire=%zu visual_confirmed=0\n",status,stats.original_bytes,stats.blocks,stats.compressed_bytes,stats.wire_bytes);
    } else if(status==WX_OK) {
        uint8_t old=0;status=wx_device_get_backlight(d,&old);
        printf("brightness_read_status=%d old_raw=%u valid=%d\n",status,(unsigned)old,status==WX_OK);
        if(status==WX_OK)status=wx_device_set_backlight_percent(d,20,500);
        printf("brightness_status=%d requested_percent=20 raw=51 fade_ms=500 automatic_restore=0 visual_confirmed=0\n",status);
    }
    if(d) {
        wx_status_t cleanup=wx_device_init_close(d,&r);
        printf("cleanup_status=%d close_errno=%d\n",cleanup,r.close_errno);
        wx_device_destroy(d);if(cleanup!=WX_OK)status=cleanup;
    }
    return status==WX_OK?0:2;
}
int main(int argc,char **argv)
{
    bool action=false,run=false,bitmap=false,limited=false;
    const char *path=NULL,*serial=NULL;
    for(int n=1;n<argc;++n) {
        if(!strcmp(argv[n],"--help")) {
            puts("weact-fs35-features [--dry-run | --fastlz-once | --brightness-once] [--device PATH] [--serial SERIAL] [--allow-unknown-occupancy]\nDefault dry-run. Independent opt-in actions. Preserve connection; no setter/Fill. FastLZ requires orientation query=2 immediately before TX. Brightness reads first; one 20%/500ms change, no automatic restore.");return 0;
        } else if(!strcmp(argv[n],"--dry-run") || !strcmp(argv[n],"--fastlz-once") || !strcmp(argv[n],"--brightness-once")) {
            if(action)return 2;
            action=true;run=strcmp(argv[n],"--dry-run")!=0;bitmap=!strcmp(argv[n],"--fastlz-once");
        } else if(!strcmp(argv[n],"--device") && !path && n+1<argc)path=argv[++n];
        else if(!strcmp(argv[n],"--serial") && !serial && n+1<argc)serial=argv[++n];
        else if(!strcmp(argv[n],"--allow-unknown-occupancy") && !limited)limited=true;
        else return 2;
    }
    for(int fd=0;fd<=2;++fd) {
        struct stat st;if(!fstat(fd,&st) && S_ISCHR(st.st_mode) && st.st_rdev==makedev(166,0))return 2;
    }
    if((path && (path[0]!='/' || strlen(path)>=4096)) || (serial && (!*serial || strlen(serial)>=64)))return 2;
    puts("FS35/2 480x320 baud=115200 guarded/exclusive deadline=2000ms; hardware support is firmware-specific; see hardware report.");
    puts("FastLZ: TX 82 0A, require RX 82 02 0A; header 15 10 00 10 00 4F 00 3F 00 0A; blocks original<=1920, uint16LE lengths + level1 data, no trailer.");
    puts("Brightness: TX 83 0A, require RX 83 raw 0A; then one TX 03 33 F4 01 0A (20%, 500ms); no exact restoration promised.");
    if(!run){puts("dry-run: no device open, no protocol I/O. Choose exactly one explicit action.");return 0;}
    if(fflush(stdout))return 2;
    wx_posix_select_options_t posix={.struct_size=sizeof(posix),
        .base={.struct_size=sizeof(wx_posix_init_options_t),.allow_unknown_occupancy=limited},.device=path,.serial=serial};
    wx_init_options_t options={.struct_size=sizeof(options),.model=WX_FS35,.orientation=2,.timeout_ms=2000};
    if(wx_posix_init_connector_select(&posix,&options.connector)!=WX_OK)return 2;
    return execute(&options,bitmap);
}
