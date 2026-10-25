/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * GPL-3.0-only CLI adapter. Existing MIT guard policy reused with attribution.
 * Serial transport only; display commands/encoding reside in libweact.
 */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#define WX_GUARD_OCCUPANCY_ONLY
#include "weact_linux_guard.h"
#include "weact_cli.h"
#include "weact_posix_init.h"
#include <sys/file.h>
#ifdef WEACT_ORIENTATION_DIAG
static void orientation_diag(const wx_init_report_t *r)
{
    if (!r || r->reason != WX_INIT_ORIENTATION_MISMATCH) return;
    size_t shown = r->rx_length < sizeof(r->rx) ? r->rx_length : sizeof(r->rx);
    fprintf(stderr, "orientation_diag expected=02 frame_len=3 rx_length=%zu rx_bytes=%zu rx=", r->rx_length, shown);
    for (size_t i=0; i<shown; ++i) fprintf(stderr, "%s%02X", i ? " " : "", r->rx[i]);
    fputc('\n', stderr);
}
#endif
int cli_model_product(const char *vid,const char *pid,const char *product)
{
    if(strcmp(vid,"1a86") || strcmp(pid,"fe0c"))return 0;
    if(!strcmp(product,"Display FS V1"))return WX_FS35;
    if(!strcmp(product,"Display FS 0.96 Inch"))return WX_FS096;
    return 0;
}
static uint64_t now_ms(void *unused)
{
    (void)unused;struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)<0 || t.tv_sec<0 || (uint64_t)t.tv_sec>UINT64_MAX/1000)return UINT64_MAX;
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int inspect(const cli_options *o,char node[PATH_MAX],struct stat *st,identity_t *id)
{
    memset(id,0,sizeof(*id));uint64_t now=now_ms(NULL);if(now>UINT64_MAX-2000)return 0;
    id->deadline_ms=now+2000;
    if(!realpath(o->port,node) || strncmp(node,"/dev/ttyACM",11) || !numeric(node+11) ||
       stat(node,st)<0 || !S_ISCHR(st->st_mode) || major(st->st_rdev)!=166)return 0;
    char path[PATH_MAX],iface[PATH_MAX],usb[PATH_MAX],driver[PATH_MAX];
    int n=snprintf(path,sizeof(path),"/sys/class/tty/%s/device",node+5);
    if(n<0 || (size_t)n>=sizeof(path) || !realpath(path,iface))return 0;
    strcpy(usb,iface);char *slash=strrchr(usb,'/');if(!slash)return 0;*slash=0;
    if(attribute(usb,"idVendor",id->vid,sizeof(id->vid)) || attribute(usb,"idProduct",id->pid,sizeof(id->pid)) ||
       attribute(usb,"product",id->product,sizeof(id->product)) || attribute(usb,"manufacturer",id->manufacturer,sizeof(id->manufacturer)))return 0;
    if(attribute(usb,"serial",id->serial,sizeof(id->serial))<0 && errno!=ENOENT)return 0;
    if(strcmp(id->manufacturer,"WeAct Studio") || (*o->serial && strcmp(id->serial,o->serial)))return 0;
    n=snprintf(path,sizeof(path),"%s/driver",iface);
    if(n<0 || (size_t)n>=sizeof(path) || !realpath(path,driver) || strcmp(driver,"/sys/bus/usb/drivers/cdc_acm"))return 0;
    char number[16],cls[16],sub[16],proto[16],dev[32],expected[32];
    snprintf(expected,sizeof(expected),"%u:%u",major(st->st_rdev),minor(st->st_rdev));
    n=snprintf(path,sizeof(path),"/sys/class/tty/%s/dev",node+5);
    if(n<0 || (size_t)n>=sizeof(path) || text_file(path,dev,sizeof(dev)) || strcmp(expected,dev) ||
       attribute(iface,"bInterfaceNumber",number,sizeof(number)) || strcmp(number,"00") ||
       attribute(iface,"bInterfaceClass",cls,sizeof(cls)) || strcmp(cls,"02") ||
       attribute(iface,"bInterfaceSubClass",sub,sizeof(sub)) || strcmp(sub,"02") ||
       attribute(iface,"bInterfaceProtocol",proto,sizeof(proto)) || strcmp(proto,"01"))return 0;
    id->rdev=st->st_rdev;return cli_model_product(id->vid,id->pid,id->product);
}
int cli_identify(const cli_options *o)
{char node[PATH_MAX];struct stat st;identity_t id;return inspect(o,node,&st,&id);}
#ifndef CLI_OCCUPANCY_SCAN
#define CLI_OCCUPANCY_SCAN users_of_port
#endif
static int occupancy(identity_t *id,int fd,int allow)
{
    int status=CLI_OCCUPANCY_SCAN(id,fd);
    if(id->busy){errno=EBUSY;return -1;}
    if(status==0)return 0;
    if(allow && !id->occupancy_blocked)return 0;
    errno=id->occupancy_errno;return -1;
}
static wx_status_t tx(void *context,const uint8_t *p,size_t len,size_t *written)
{
    *written=0;if(len>(size_t)SSIZE_MAX)len=(size_t)SSIZE_MAX;
    ssize_t n=write(*(int *)context,p,len);if(n>=0){*written=(size_t)n;return WX_OK;}
    return errno==EINTR?WX_INTERRUPTED:errno==EAGAIN || errno==EWOULDBLOCK?WX_AGAIN:WX_IO;
}
static wx_status_t wait_tx(void *context,uint64_t deadline)
{
    uint64_t now=now_ms(NULL);if(now>=deadline)return WX_TIMEOUT;
    uint64_t remaining=deadline-now;struct pollfd p={.fd=*(int *)context,.events=POLLOUT};
    int n=poll(&p,1,remaining>INT_MAX?INT_MAX:(int)remaining);
    if(n<0)return errno==EINTR?WX_INTERRUPTED:WX_IO;
    if(now_ms(NULL)>=deadline)return WX_TIMEOUT;
    if(p.revents&(POLLHUP|POLLERR|POLLNVAL))return WX_DISCONNECTED;
    return n && (p.revents&POLLOUT)?WX_OK:WX_AGAIN;
}
static int same(const struct stat *a,const struct stat *b)
{return S_ISCHR(b->st_mode) && a->st_rdev==b->st_rdev && a->st_ino==b->st_ino && a->st_dev==b->st_dev;}
int cli_connect(const cli_options *o,int model,wx_device_t **out,int *fd)
{
    *fd=-1;*out=NULL;
    if(model==WX_FS35) {
        wx_posix_select_options_t p={.struct_size=sizeof(p),.base={.struct_size=sizeof(p.base),.allow_unknown_occupancy=o->allow_unknown},
                                     .device=o->port,.serial=*o->serial?o->serial:NULL};
        wx_init_options_t options={.struct_size=sizeof(options),.model=WX_FS35,.orientation=2,.timeout_ms=2000};
        wx_init_report_t report={.struct_size=sizeof(report)};
        wx_status_t st=wx_posix_init_connector_select(&p,&options.connector);
        int preserve=o->code_format || (o->overlay && !o->cls);
        if(st==WX_OK)st=preserve?wx_device_init_preserve(&options,out,&report):wx_device_init(&options,out,&report);
        if(st==WX_OK && preserve)st=wx_device_confirm_orientation(*out,&report);
        if(o->verbose || st!=WX_OK)fprintf(stderr,"init_status=%d reason=%d errno=%d occupancy=%d\n",st,report.reason,report.primary_errno,report.occupancy);
#ifdef WEACT_ORIENTATION_DIAG
        orientation_diag(&report);
#endif
        return st==WX_OK?0:-1;
    }
    char node[PATH_MAX],again[PATH_MAX];struct stat before,opened,after;identity_t id;
    if(model!=WX_FS096 || inspect(o,node,&before,&id)!=WX_FS096 || occupancy(&id,-1,o->allow_unknown))return -1;
    *fd=open(node,O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);if(*fd<0)return -1;
    if(fstat(*fd,&opened) || !same(&before,&opened) ||
       inspect(o,again,&after,&id)!=WX_FS096 || strcmp(node,again) || !same(&opened,&after) ||
       flock(*fd,LOCK_EX|LOCK_NB) || ioctl(*fd,TIOCEXCL) || occupancy(&id,*fd,o->allow_unknown))goto failed;
    struct termios tty;if(tcgetattr(*fd,&tty))goto failed;cfmakeraw(&tty);
    tty.c_cflag&=~(CSIZE|PARENB|PARODD|CSTOPB|HUPCL|CRTSCTS);tty.c_cflag|=CS8|CLOCAL|CREAD;
    tty.c_iflag&=~(IXON|IXOFF|IXANY);tty.c_cc[VMIN]=0;tty.c_cc[VTIME]=0;
    if(cfsetispeed(&tty,B115200) || cfsetospeed(&tty,B115200) || tcsetattr(*fd,TCSANOW,&tty) ||
       inspect(o,again,&after,&id)!=WX_FS096 || !same(&opened,&after) || occupancy(&id,*fd,o->allow_unknown))goto failed;
    wx_device_options_t options={.struct_size=sizeof(options),.model=WX_FS096,.width=160,.height=80,.orientation=2,.timeout_ms=5000,
        .transport={.context=fd,.write=tx,.now_ms=now_ms,.wait_writable=wait_tx}};
    if(wx_device_create(&options,out)!=WX_OK)goto failed;
    /* Overlay and code modes preserve orientation; no speculative FS096
     * readback. User must already have landscape. Ordinary text sets legacy2. */
    if(!o->code_format && (!o->overlay || o->cls)) {
        if(wx_device_set_orientation(*out,2)!=WX_OK)goto failed;
        struct timespec delay={.tv_nsec=100000000};while(nanosleep(&delay,&delay)<0)if(errno!=EINTR)goto failed;
    }
    return 0;
failed:
    wx_device_destroy(*out);*out=NULL;close(*fd);*fd=-1;return -1;
}
int cli_disconnect(wx_device_t *d,int fd)
{
    int result=0;if(d && fd<0 && wx_device_init_close(d,NULL)!=WX_OK)result=-1;
    wx_device_destroy(d);if(fd>=0 && close(fd)<0)result=-1;return result;
}
