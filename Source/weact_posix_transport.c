/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "weact_posix_transport.h"
#include "weact_posix_init.h"
#include "weact_linux_guard.h"
#include "weact_linux_select.h"
#include "weact_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

struct wx_posix_transport {
    int fd, error;
    bool failed, clock_failed, exclusive, allow_unknown;
    bool portable,auto_select;dev_t expected_rdev;char node[PATH_MAX],serial[64],filter_serial[64];
    wx_allocator_t allocator;
};

static wx_status_t error_status(int error)
{
    if (error == EINTR) return WX_INTERRUPTED;
    if (error == EAGAIN || error == EWOULDBLOCK) return WX_AGAIN;
    if (error == EIO || error == ENODEV || error == ENXIO || error == EPIPE || error == EBADF)
        return WX_DISCONNECTED;
    return WX_IO;
}
static wx_status_t terminal(wx_posix_transport_t *p, int error)
{
    p->failed = true;
    p->error = error;
    return error_status(error);
}
static bool monotonic_ms(uint64_t *out)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return false;
    if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L) {
        errno = EIO;
        return false;
    }
    uint64_t sec = (uint64_t)ts.tv_sec, ms = (uint64_t)ts.tv_nsec / 1000000;
    *out = sec > (UINT64_MAX-ms)/1000 ? UINT64_MAX : sec*1000+ms;
    return true;
}
static uint64_t now_ms(void *context)
{
    wx_posix_transport_t *p = context;
    uint64_t value;
    if (p->clock_failed) return UINT64_MAX;
    /* The borrowed clock API has no status channel. MAX forces the device's
     * next deadline check to fail closed; last_errno identifies clock errors. */
    if (!monotonic_ms(&value)) {
        p->clock_failed = true;
        (void)terminal(p, errno);
        return UINT64_MAX;
    }
    return value;
}
static wx_status_t write_bytes(void *context, const uint8_t *bytes, size_t length,
                                size_t *written)
{
    wx_posix_transport_t *p = context;
    if (!written) return WX_INVALID;
    *written = 0;
    if (p->fd < 0) return WX_DISCONNECTED;
    if (p->failed) return WX_STREAM_FAILED;
    if (!bytes && length) return WX_INVALID;
    if (!length) return WX_OK;
    size_t chunk = length > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : length;
    ssize_t result = write(p->fd, bytes, chunk);
    if (result >= 0) {
        *written = (size_t)result;
        return WX_OK;
    }
    int error = errno;
    wx_status_t status = error_status(error);
    if (status == WX_INTERRUPTED || status == WX_AGAIN) return status;
    return terminal(p, error);
}
static wx_status_t wait_writable(void *context, uint64_t end)
{
    wx_posix_transport_t *p = context;
    if (p->fd < 0) return WX_DISCONNECTED;
    if (p->failed) return WX_STREAM_FAILED;
    uint64_t now = now_ms(p);
    if (p->failed) return WX_IO;
    if (now >= end) return WX_TIMEOUT;
    uint64_t remaining = end-now;
    int timeout = remaining > INT_MAX ? INT_MAX : (int)remaining;
    struct pollfd fd = {.fd=p->fd, .events=POLLOUT};
    int result = poll(&fd, 1, timeout);
    if (result < 0) {
        int error = errno;
        if (error == EINTR) return WX_INTERRUPTED;
        return terminal(p, error);
    }
    now = now_ms(p);
    if (p->failed) return WX_IO;
    if (now >= end) return WX_TIMEOUT;
    if (!result) return WX_AGAIN; /* capped or early poll expiry; same deadline */
    if (fd.revents & POLLNVAL) return terminal(p, EBADF);
    if (fd.revents & (POLLHUP|POLLERR)) return terminal(p, ENODEV);
    if (fd.revents & POLLOUT) return WX_OK;
    return terminal(p, EIO);
}
wx_status_t wx_posix_transport_open(const char *path, const wx_posix_options_t *o,
                                   wx_posix_transport_t **out)
{
    if (!out) { errno=EINVAL; return WX_INVALID; }
    *out=NULL;
    if (!path || !*path || !o || o->struct_size != sizeof(*o) ||
        !wx_allocator_valid(&o->allocator)) { errno=EINVAL; return WX_INVALID; }
    wx_posix_transport_t *p=wx_allocate(&o->allocator,sizeof(*p));
    if (!p) { errno=ENOMEM; return WX_NOMEM; }
    *p=(wx_posix_transport_t){.fd=-1,.allocator=o->allocator};
    uint64_t probe;
    if (!monotonic_ms(&probe)) {
        int error=errno;
        wx_release(&p->allocator,p);
        errno=error;
        return WX_IO;
    }
    p->fd=open(path,O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
    if (p->fd < 0) goto failure;
    struct termios tty;
    if (tcgetattr(p->fd,&tty) < 0) goto failure;
    cfmakeraw(&tty);
    tty.c_iflag &= ~(IXON|IXOFF|IXANY|INPCK|IGNPAR);
    tty.c_cflag &= ~(CSIZE|PARENB|PARODD|CSTOPB|HUPCL);
#ifdef CMSPAR
    tty.c_cflag &= ~CMSPAR;
#endif
    tty.c_cflag |= CS8|CLOCAL|CREAD;
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
    if (o->rtscts) tty.c_cflag |= CRTSCTS;
#else
    if (o->rtscts) { errno=ENOTSUP; goto failure; }
#endif
    if (o->hangup_on_close) tty.c_cflag |= HUPCL;
    tty.c_cc[VMIN]=0;
    tty.c_cc[VTIME]=0;
    if (cfsetispeed(&tty,B115200) < 0 || cfsetospeed(&tty,B115200) < 0 ||
        tcsetattr(p->fd,TCSANOW,&tty) < 0) goto failure;
    *out=p;
    return WX_OK;
failure: {
        int error=errno;
        if (p->fd >= 0) (void)close(p->fd); /* Linux close is never retried */
        wx_release(&p->allocator,p);
        errno=error;
        return error_status(error);
    }
}
wx_transport_t wx_posix_transport_bind(wx_posix_transport_t *p)
{
    return p ? (wx_transport_t){p,write_bytes,now_ms,wait_writable} : (wx_transport_t){0};
}
wx_status_t wx_posix_transport_close(wx_posix_transport_t *p)
{
    if (!p) return WX_INVALID;
    if (p->fd < 0) return WX_OK;
    int fd=p->fd;
    p->fd=-1;
    p->failed=true;
    if (close(fd) < 0) { p->error=errno; return WX_IO; }
    return WX_OK;
}
void wx_posix_transport_destroy(wx_posix_transport_t *p)
{
    if (!p) return;
    (void)wx_posix_transport_close(p);
    wx_allocator_t allocator=p->allocator;
    wx_release(&allocator,p);
}
int wx_posix_transport_last_errno(const wx_posix_transport_t *p)
{
    return p ? p->error : EINVAL;
}

/* Guarded managed entry. No alternate path, fd adoption or identity bypass. */
static wx_status_t connector_clock(void *context,uint64_t *out)
{
    (void)context;
    return monotonic_ms(out) ? WX_OK : WX_IO;
}
static wx_status_t check_identity(void *context,wx_init_stage_t stage,uint64_t end,wx_init_report_t *r)
{
    wx_posix_transport_t *p=context;
    identity_t id={0};
    (void)stage;
    if (now_ms(p)>=end) return WX_TIMEOUT;
    int result=p->portable ? collect_selected_identity(p->node,p->serial[0]?p->serial:NULL,&id,p->fd,end) :
        collect_identity(NULL,&id,p->fd,end);
    /* Auto-selection must remain unique if another display appears between
     * discovery/open or later guards. Re-enumeration never opens a node. */
    if(result==0 && p->portable && p->auto_select) {
        identity_t selected={0};char node[PATH_MAX];
        if(select_device_at("/sys/class/tty",NULL,p->filter_serial[0]?p->filter_serial:NULL,
            &selected,node,sizeof(node),end)<0)result=-1;
        else if(strcmp(node,p->node) || selected.rdev!=p->expected_rdev || strcmp(selected.serial,p->serial)) {
            errno=ENODEV;result=-1;
        }
    }
#define COPY(field) snprintf(r->field,sizeof(r->field),"%.*s",(int)sizeof(r->field)-1,id.field)
    COPY(vid);COPY(pid);COPY(serial);COPY(manufacturer);COPY(product);COPY(driver);
    COPY(occupancy_path);COPY(occupancy_operation);COPY(holder_path);
#undef COPY
    snprintf(r->node,sizeof(r->node),"%.*s",(int)sizeof(r->node)-1,p->node);
    r->device_rdev=p->expected_rdev;
    r->allow_unknown_occupancy=p->allow_unknown;
    r->occupancy=id.busy ? WX_INIT_OCCUPANCY_BUSY : id.complete ?
        WX_INIT_OCCUPANCY_CLEAR_VISIBLE : WX_INIT_OCCUPANCY_INCONCLUSIVE;
    if (result<0 || !(p->portable ? portable_identity_matches(&id,p->serial[0]?p->serial:NULL) && id.rdev==p->expected_rdev : fs35_identity_matches(&id)) || !id.nodes_ok) {
        r->reason=WX_INIT_IDENTITY_MISMATCH;return WX_INVALID;
    }
    if (p->fd>=0) {
        struct stat st;
        if (fstat(p->fd,&st)<0 || !S_ISCHR(st.st_mode) || st.st_rdev!=p->expected_rdev) {
            r->reason=WX_INIT_IDENTITY_MISMATCH;return WX_INVALID;
        }
    }
    if (id.busy) {r->reason=WX_INIT_PORT_BUSY;errno=EBUSY;return WX_INVALID;}
    if (!id.complete) {
        if (!p->allow_unknown || id.occupancy_blocked ||
            (id.occupancy_errno!=EPERM && id.occupancy_errno!=EACCES)) {
            r->reason=WX_INIT_OCCUPANCY_UNKNOWN;errno=id.occupancy_errno;return WX_INVALID;
        }
        r->unknown_accepted=true;
    }
    return now_ms(p)>=end ? WX_TIMEOUT : WX_OK;
}
static wx_status_t wait_input(void *context,uint64_t end)
{
    wx_posix_transport_t *p=context;
    if (p->failed || p->fd<0) return WX_STREAM_FAILED;
    uint64_t now=now_ms(p);
    if (now>=end) return WX_TIMEOUT;
    uint64_t delta=end-now;
    struct pollfd fd={.fd=p->fd,.events=POLLIN};
    int result=poll(&fd,1,delta>INT_MAX ? INT_MAX : (int)delta);
    if (result<0) return error_status(errno);
    if (now_ms(p)>=end) return WX_TIMEOUT;
    if (fd.revents&(POLLHUP|POLLERR|POLLNVAL)) return terminal(p,ENODEV);
    return result && (fd.revents&POLLIN) ? WX_OK : WX_AGAIN;
}
static wx_status_t read_input(void *context,uint8_t *bytes,size_t n,size_t *received)
{
    wx_posix_transport_t *p=context;*received=0;
    if (p->failed || p->fd<0) return WX_STREAM_FAILED;
    ssize_t result=read(p->fd,bytes,n);
    if (result<0) return error_status(errno);
    *received=(size_t)result;return WX_OK;
}
static wx_status_t probe_input(void *context,uint64_t end)
{
    wx_posix_transport_t *p=context;
    if (p->failed || p->fd<0) return WX_STREAM_FAILED;
    if (now_ms(p)>=end) return WX_TIMEOUT;
    struct pollfd fd={.fd=p->fd,.events=POLLIN};
    int result=poll(&fd,1,0);
    if (result<0) return error_status(errno);
    return result || fd.revents ? WX_IO : WX_OK;
}
static void seal_channel(void *context)
{ ((wx_posix_transport_t *)context)->failed=true; }
static wx_status_t release_channel(void *context,wx_init_report_t *r)
{
    wx_posix_transport_t *p=context;
    if (p->exclusive) {
        p->exclusive=false;
        if (ioctl(p->fd,TIOCNXCL)<0) r->release_errno=errno;
    }
    r->close_called=p->fd>=0;
    wx_status_t status=wx_posix_transport_close(p);
    r->close_succeeded=r->close_called && status==WX_OK;
    if (status!=WX_OK) r->close_errno=p->error;
    if (r->release_errno) status=WX_IO;
    wx_posix_transport_destroy(p);
    return status;
}
static wx_status_t acquire_options(const wx_posix_init_options_t *o,const wx_posix_select_options_t *selection,
                                   uint64_t end,wx_init_channel_t *out,wx_init_report_t *r)
{
    *out=(wx_init_channel_t){0};
    wx_posix_transport_t *p=wx_allocate(&o->allocator,sizeof(*p));
    if (!p) return WX_NOMEM;
    *p=(wx_posix_transport_t){.fd=-1,.allocator=o->allocator,.allow_unknown=o->allow_unknown_occupancy};
    p->expected_rdev=makedev(166,0);snprintf(p->node,sizeof(p->node),"%s",TARGET);
    wx_status_t status;
    if(selection) {
        identity_t id={0};p->portable=true;p->auto_select=selection->device==NULL;
        if(selection->serial)snprintf(p->filter_serial,sizeof(p->filter_serial),"%s",selection->serial);
        if(select_device_at("/sys/class/tty",selection->device,selection->serial,&id,p->node,sizeof(p->node),end)<0) {
            status=WX_INVALID;r->reason=WX_INIT_IDENTITY_MISMATCH;goto failed;
        }
        p->expected_rdev=id.rdev;snprintf(p->serial,sizeof(p->serial),"%s",id.serial);
    }
    status=check_identity(p,WX_INIT_STAGE_ACQUIRE,end,r);
    if (status!=WX_OK) goto failed;
    p->fd=open(p->node,O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
    if (p->fd<0) {status=error_status(errno);goto failed;}
    r->open_succeeded=true;
    struct stat st;
    if (fstat(p->fd,&st)<0 || !S_ISCHR(st.st_mode) || st.st_rdev!=p->expected_rdev) {
        r->reason=WX_INIT_IDENTITY_MISMATCH;status=WX_INVALID;goto failed;
    }
    if (ioctl(p->fd,TIOCEXCL)<0) {
        r->reason=WX_INIT_EXCLUSIVE_FAILED;status=WX_IO;goto failed;
    }
    p->exclusive=true;r->exclusive_succeeded=true;
    status=check_identity(p,WX_INIT_STAGE_ACQUIRE,end,r);
    if (status!=WX_OK) goto failed;
    struct termios tty;
    if (tcgetattr(p->fd,&tty)<0) goto configure_failed;
    cfmakeraw(&tty);
    tty.c_iflag &= ~(IXON|IXOFF|IXANY|INPCK|IGNPAR);
    tty.c_cflag &= ~(CSIZE|PARENB|PARODD|CSTOPB|CRTSCTS|HUPCL);
#ifdef CMSPAR
    tty.c_cflag &= ~CMSPAR;
#endif
    tty.c_cflag |= CS8|CLOCAL|CREAD;tty.c_cc[VMIN]=tty.c_cc[VTIME]=0;
    if (cfsetispeed(&tty,B115200)<0 || cfsetospeed(&tty,B115200)<0 ||
        tcsetattr(p->fd,TCSANOW,&tty)<0) goto configure_failed;
    r->configure_succeeded=true;r->baud=115200;r->raw_8n1=r->flow_control_off=true;
    if (now_ms(p)>=end) {status=WX_TIMEOUT;goto failed;}
    *out=(wx_init_channel_t){.struct_size=sizeof(*out),.tx=wx_posix_transport_bind(p),
        .read=read_input,.wait_readable=wait_input,.probe_quiet=probe_input,
        .check_guards=check_identity,.seal=seal_channel,.release=release_channel};
    return WX_OK;
configure_failed:
    r->reason=WX_INIT_CONFIGURE_FAILED;status=WX_IO;
failed: {
    int saved=errno;
    if (now_ms(p)>=end) {status=WX_TIMEOUT;r->reason=WX_INIT_DEADLINE;}
    r->cleanup_status=release_channel(p,r);errno=saved;r->primary_errno=saved;
    return status;
    }
}
static wx_status_t guarded_acquire(void *context,uint64_t end,wx_init_channel_t *out,wx_init_report_t *r)
{return acquire_options(context,NULL,end,out,r);}
static wx_status_t selected_acquire(void *context,uint64_t end,wx_init_channel_t *out,wx_init_report_t *r)
{const wx_posix_select_options_t *o=context;return acquire_options(&o->base,o,end,out,r);}
wx_status_t wx_posix_init_connector_select(const wx_posix_select_options_t *o,wx_init_connector_t *out)
{
    if(!o || !out || o->struct_size!=sizeof(*o) || o->base.struct_size!=sizeof(o->base) ||
       !wx_allocator_valid(&o->base.allocator) || (o->device && (o->device[0]!='/' || strlen(o->device)>=PATH_MAX)) ||
       (o->serial && (!o->serial[0] || strlen(o->serial)>=64)))return WX_INVALID;
    *out=(wx_init_connector_t){sizeof(*out),(void *)o,connector_clock,selected_acquire};return WX_OK;
}
wx_status_t wx_posix_init_connector(const wx_posix_init_options_t *o,wx_init_connector_t *out)
{
    if (!o || !out || o->struct_size!=sizeof(*o) || !wx_allocator_valid(&o->allocator))
        return WX_INVALID;
    *out=(wx_init_connector_t){sizeof(*out),(void *)o,connector_clock,guarded_acquire};
    return WX_OK;
}
