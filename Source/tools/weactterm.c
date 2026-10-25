/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 * Minimal foreground FS35 terminal. No hardware access without --run.
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "weact_posix_init.h"
#include "weact_fs35.h"
#include "weactterm_input.h"
#include "weactterm_presenter.h"
#include "weactterm_pty.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t stopped,resized;
static void signal_event(int sig)
{if(sig==SIGWINCH)resized=1;else if(sig!=SIGCHLD)stopped=sig;}
static uint64_t milliseconds(void)
{
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return UINT64_MAX;
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int safe_stdio(void)
{
    for(int fd=0;fd<=2;++fd) {
        struct stat st;
        if(fstat(fd,&st)==0 && S_ISCHR(st.st_mode) && st.st_rdev==makedev(166,0))return -1;
    }
    return 0;
}
static int number(const char *text,unsigned limit,unsigned *out)
{
    if(!text || !*text)return -1;
    unsigned value=0;
    for(;*text;++text){if(*text<'0' || *text>'9' || value>limit/10)return -1;value=value*10+(unsigned)(*text-'0');if(value>limit)return -1;}
    *out=value;return 0;
}
static void usage(void)
{
    puts("WeActTerm FS35: [--dry-run | --run] [--allow-unknown-occupancy] [--command 'shell command'] [--ctrl-c-to-pty] [--perf-stats] [--device /dev/...] [--serial SERIAL] [--font-size tiny|small|normal|large|xlarge] [--compression off|fastlz] [--brightness 0..50] [--brightness-fade-ms 0..65535]\n"
         "Default: finite memory-only PTY demo. --run: interactive $SHELL on display.\n"
         "Ctrl+C exits by default; --ctrl-c-to-pty forwards it. Ctrl+] always exits.\n"
         "Compression defaults off. Brightness unchanged unless specified; fade capped at 5000 ms.\n"
         "Default normal: 60x20 cells (8x16), DejaVu Sans Mono, TERM=xterm. USB auto-selects a single FS35; --device or --serial disambiguates.");
}
static void dump_screen(wt_screen *s)
{
    char text[WT_MAX_ROWS*(WT_MAX_COLS*VTERM_MAX_CHARS_PER_CELL*4+1)];
    size_t n=vterm_screen_get_text(s->screen,text,sizeof(text),(VTermRect){0,s->rows,0,s->cols});
    if(n>sizeof(text))n=sizeof(text);
    printf("font=%s cell=%dx%d grid=%dx%d\n",s->font->name,s->font->width,s->font->height,s->cols,s->rows);
    puts("--- dry-run screen ---");fwrite(text,1,n,stdout);putchar('\n');
}
int main(int argc,char **argv)
{
    if(safe_stdio()<0)return 2;
    bool run=false,limited=false,action=false,forward_ctrl_c=false,perf=false;
    const char *command=NULL,*device_path=NULL,*serial=NULL;
    wt_font_size font_size=WT_FONT_NORMAL;bool font_selected=false;
    bool fastlz=false,compression_set=false,brightness_set=false,fade_set=false;
    unsigned brightness=0,fade=1000;
    for(int n=1;n<argc;++n) {
        if(!strcmp(argv[n],"--help")){usage();return 0;}
        else if(!strcmp(argv[n],"--dry-run") || !strcmp(argv[n],"--run")) {
            if(action)return 2;
            action=true;run=!strcmp(argv[n],"--run");
        } else if(!strcmp(argv[n],"--allow-unknown-occupancy") && !limited)limited=true;
        else if(!strcmp(argv[n],"--command") && !command && n+1<argc)command=argv[++n];
        else if(!strcmp(argv[n],"--device") && !device_path && n+1<argc)device_path=argv[++n];
        else if(!strcmp(argv[n],"--serial") && !serial && n+1<argc)serial=argv[++n];
        else if(!strcmp(argv[n],"--font-size") && !font_selected && n+1<argc) {
            if(wt_font_parse(argv[++n],&font_size)<0)return 2;
            font_selected=true;
        }
        else if(!strcmp(argv[n],"--compression") && !compression_set && n+1<argc) {
            const char *mode=argv[++n];if(strcmp(mode,"off") && strcmp(mode,"fastlz"))return 2;
            fastlz=!strcmp(mode,"fastlz");compression_set=true;
        } else if(!strcmp(argv[n],"--brightness") && !brightness_set && n+1<argc) {
            if(number(argv[++n],50,&brightness)<0)return 2;
            brightness_set=true;
        } else if(!strcmp(argv[n],"--brightness-fade-ms") && !fade_set && n+1<argc) {
            if(number(argv[++n],65535,&fade)<0)return 2;
            fade_set=true;
        }
        else if(!strcmp(argv[n],"--perf-stats") && !perf)perf=true;
        else if(!strcmp(argv[n],"--ctrl-c-to-pty") && !forward_ctrl_c)forward_ctrl_c=true;
        else {usage();return 2;}
    }
    if(fade_set && !brightness_set)return 2;
    if(compression_set || brightness_set)fprintf(stderr,"compression=%s brightness=%s percent=%u raw=%u fade_requested_ms=%u fade_effective_ms=%u visual_confirmed=0\n",fastlz?"fastlz":"off",brightness_set?"explicit":"unchanged",brightness,brightness*255/100,fade,fade>5000?5000:fade);
    if((device_path && (device_path[0]!='/' || strlen(device_path)>=4096)) ||
       (serial && (!serial[0] || strlen(serial)>=64)))return 2;
    if(!run && !command)command="printf 'WeActTerm dry-run\\r\\nПривет, мир!\\r\\n\\033[31mRed \\033[32mGreen\\033[0m\\r\\n'";
    wt_screen *screen=calloc(1,sizeof(*screen));
    if(!screen || wt_screen_open_size(screen,font_size)<0){free(screen);fputs("libvterm/font initialization failed\n",stderr);return 2;}
    screen->perf_enabled=perf;
    uint64_t pty_read_ns=0;
    wx_surface_t *canvas=NULL;wx_device_t *device=NULL;
    wt_presenter presenter={0};wt_pty pty={.master=-1,.pid=-1};
    struct termios saved_tty;bool tty_changed=false;int saved_flags=-1;
    int result=2;
    const int signals[]={SIGINT,SIGTERM,SIGHUP,SIGCHLD,SIGWINCH};
    struct sigaction sa={.sa_handler=signal_event};sigemptyset(&sa.sa_mask);
    for(size_t n=0;n<sizeof(signals)/sizeof(signals[0]);++n)if(sigaction(signals[n],&sa,NULL)<0)goto done;
    signal(SIGPIPE,SIG_IGN);
    if(wx_surface_create(480,320,0,WX_RGB565_LE,NULL,&canvas)!=WX_OK)goto done;
    if(run) {
        wx_posix_select_options_t posix={.struct_size=sizeof(posix),
            .base={.struct_size=sizeof(wx_posix_init_options_t),.allow_unknown_occupancy=limited},
            .device=device_path,.serial=serial};
        wx_init_options_t options={.struct_size=sizeof(options),.model=WX_FS35,.orientation=2,.timeout_ms=2000};
        wx_init_report_t report={.struct_size=sizeof(report)};
        wx_status_t status=wx_posix_init_connector_select(&posix,&options.connector);
        if(status==WX_OK)status=wx_device_init(&options,&device,&report);
        fprintf(stderr,"init_status=%d reason=%d errno=%d matching=%d fill_complete=%d\n",status,report.reason,report.primary_errno,report.orientation_matching,report.fill_tx_complete);
        if(status!=WX_OK) {
            if(report.primary_errno==ENOTUNIQ)fputs("Multiple matching FS35 devices: use --device or --serial\n",stderr);
            goto done;
        }
    }
    if(run && brightness_set && wx_device_set_backlight_percent(device,brightness,(uint16_t)fade)!=WX_OK) {
        fputs("brightness TX failed; stopping without retry\n",stderr);goto done;
    }
    if(stopped){result=128+stopped;goto done;}
    if(wt_pty_open_size(&pty,command,screen->rows,screen->cols)<0){perror("forkpty");goto done;}
    if(isatty(STDIN_FILENO)) {
        if(tcgetattr(STDIN_FILENO,&saved_tty)<0)goto done;
        struct termios raw=saved_tty;cfmakeraw(&raw);
        if(tcsetattr(STDIN_FILENO,TCSANOW,&raw)<0)goto done;
        tty_changed=true;
    }
    saved_flags=fcntl(STDIN_FILENO,F_GETFL);
    if(saved_flags<0 || fcntl(STDIN_FILENO,F_SETFL,saved_flags|O_NONBLOCK)<0)goto done;
    if(run && wt_presenter_open_mode(&presenter,device,perf,fastlz)<0)goto done;
    fprintf(stderr,"mode=%s PTY=%ld font=%s cell=%dx%d grid=%dx%d canvas=480x320 TERM=xterm max_fps=20 Ctrl+C=%s Ctrl+]=exit\n",
        run?"display":"dry-run/no-device",(long)pty.pid,screen->font->name,screen->font->width,screen->font->height,screen->cols,screen->rows,forward_ctrl_c?"PTY":"exit");
    bool input_eof=false,pty_eof=false;
    wt_input input={.ctrl_c_to_pty=forward_ctrl_c};
    uint64_t next_frame=0,exit_seen=0;
    result=0;
    while(!stopped && !input.stop) {
        uint64_t now=milliseconds();if(now==UINT64_MAX){result=2;break;}
        if(resized){resized=0;if(wt_pty_resize(&pty)<0){result=2;break;}}
        wt_input_tick(&input,screen,now);
        if(screen->queue_failed){result=2;break;}
        if(run && wt_presenter_status(&presenter)!=WX_OK){result=2;break;}
        if(screen->dirty && (now>=next_frame || pty_eof)) {
            if(wt_screen_render(screen,canvas)<0 || (run && wt_presenter_submit(&presenter,canvas)<0)){result=2;break;}
            next_frame=now+50;
        }
        int reaped=wt_pty_reap(&pty);
        if(reaped<0){result=2;break;}
        if(reaped && !exit_seen)exit_seen=now;
        if(pty_eof || (exit_seen && now-exit_seen>=500))break;
        struct pollfd fds[2]={{.fd=pty.master,.events=POLLIN|(screen->queued?POLLOUT:0)},
                             {.fd=input_eof?-1:STDIN_FILENO,.events=POLLIN}};
        int ready=poll(fds,2,20);
        if(ready<0){if(errno==EINTR)continue;result=2;break;}
        if(fds[0].revents&POLLNVAL){result=2;break;}
        if(fds[0].revents&(POLLIN|POLLHUP|POLLERR)) {
            for(unsigned reads=0;reads<4;++reads) {
                char bytes[4096];uint64_t read_start=perf?wt_now_ns():0;
                ssize_t n=read(pty.master,bytes,sizeof(bytes));
                int read_errno=errno;
                if(perf)pty_read_ns+=wt_elapsed_ns(read_start);
                errno=read_errno;
                if(n>0){if(wt_screen_feed(screen,bytes,(size_t)n)<0){result=2;pty_eof=true;break;}continue;}
                if(!n || (n<0 && errno==EIO)){pty_eof=true;break;}
                if(errno==EINTR)continue;
                if(errno!=EAGAIN && errno!=EWOULDBLOCK){result=2;pty_eof=true;}
                break;
            }
        }
        if(fds[1].revents&(POLLIN|POLLHUP|POLLERR|POLLNVAL)) {
            char bytes[1024];ssize_t n=read(STDIN_FILENO,bytes,sizeof(bytes));
            if(n>0)wt_input_bytes(&input,screen,bytes,(size_t)n,now);
            else if(!n){input_eof=true;wt_screen_queue(screen,"\004",1);}
            else if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){result=2;break;}
        }
        if(!pty_eof && screen->queued && (fds[0].revents&POLLOUT)) {
            ssize_t n=write(pty.master,screen->outgoing,screen->queued);
            if(n>0){screen->queued-=(size_t)n;memmove(screen->outgoing,screen->outgoing+n,screen->queued);}
            else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){result=2;break;}
        }
    }
    if(!result && screen->dirty && !stopped && !input.stop) {
        if(wt_screen_render(screen,canvas)<0 || (run && wt_presenter_submit(&presenter,canvas)<0))result=2;
    }
    if(!run)dump_screen(screen);
    if(!result && pty.reaped)result=WIFEXITED(pty.status)?WEXITSTATUS(pty.status):128+WTERMSIG(pty.status);
    if(stopped)result=128+stopped;
    if(input.stop)result=130;
done:
    if(tty_changed && tcsetattr(STDIN_FILENO,TCSANOW,&saved_tty)<0)result=2;
    if(saved_flags>=0 && fcntl(STDIN_FILENO,F_SETFL,saved_flags)<0)result=2;
    wt_pty_close(&pty);
    if(!result && pty.reaped)result=WIFEXITED(pty.status)?WEXITSTATUS(pty.status):128+WTERMSIG(pty.status);
    wx_status_t bitmap_status=wt_presenter_close(&presenter);
    if(bitmap_status!=WX_OK)result=2;
    if(device) {
        wx_init_report_t report={.struct_size=sizeof(report)};
        wx_status_t cleanup=wx_device_init_close(device,&report);
        fprintf(stderr,"bitmap_status=%d bitmap_frames=%zu cleanup_status=%d release_errno=%d close_errno=%d close_succeeded=%d visual_confirmed=0\n",
            bitmap_status,presenter.sent,cleanup,report.release_errno,report.close_errno,report.close_succeeded);
        wx_device_destroy(device);if(cleanup!=WX_OK)result=2;
    }
    if(perf) {
        fprintf(stderr,"perf_stats pty_read_ns=%llu vt_ns=%llu glyph_raster_ns=%llu framebuffer_ns=%llu render_calls=%llu cells_drawn=%llu glyph_misses=%llu\n",
            (unsigned long long)pty_read_ns,(unsigned long long)screen->vt_ns,(unsigned long long)screen->glyph_ns,
            (unsigned long long)screen->framebuffer_ns,(unsigned long long)screen->render_calls,
            (unsigned long long)screen->cells_drawn,(unsigned long long)screen->glyph_misses);
        fprintf(stderr,"perf_stats queue_wait_ns=%llu bitmap_ns=%llu tx_wait_ns=%llu tx_write_ns=%llu snapshot_copy_ns=%llu diff_ns=%llu updates=%zu bitmap_regions=%zu full_frames=%zu skipped=%zu pixel_bytes=%zu header_bytes=%zu full_baseline=%zu full_borrowed=%zu full_cost=%zu region_merges=%zu update_attempts=%zu full_equivalent_pixel_bytes=%.0f pixel_ratio=%.6f original_pixel_bytes=%zu compressed_bytes=%zu compressed_block_bytes=%zu wire_bytes=%zu compression_ratio=%.6f compression_ns=%llu transfer_ns=%llu compressed_regions=%zu compressed_frames=%zu lcd_time_measured=0\n",
            (unsigned long long)presenter.queue_wait_ns,(unsigned long long)presenter.bitmap_ns,
            (unsigned long long)presenter.tx_wait_ns,(unsigned long long)presenter.tx_write_ns,
            (unsigned long long)presenter.copy_ns,(unsigned long long)presenter.diff_ns,presenter.sent,
            presenter.regions,presenter.full_frames,presenter.skipped,presenter.pixel_bytes,presenter.header_bytes,
            presenter.full_baseline,presenter.full_borrowed,presenter.full_cost,presenter.region_merges,
            presenter.attempted,(double)presenter.attempted*307200.0,
            presenter.attempted?(double)presenter.original_bytes/((double)presenter.attempted*307200.0):0.0,
            presenter.original_bytes,presenter.compressed_bytes,presenter.compressed_block_bytes,presenter.wire_bytes,
            presenter.original_bytes?(fastlz?(double)presenter.compressed_block_bytes/(double)presenter.original_bytes:1.0):0.0,
            (unsigned long long)presenter.compression_ns,(unsigned long long)presenter.transfer_ns,presenter.compressed_regions,presenter.compressed_frames);
    }
    wx_surface_destroy(canvas);wt_screen_close(screen);free(screen);
    fprintf(stderr,"weactterm_exit=%d\n",result);return result;
}
