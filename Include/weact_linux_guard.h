#ifndef WEACT_LINUX_GUARD_H
#define WEACT_LINUX_GUARD_H
/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT). Private bounded Linux identity and occupancy policy.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TARGET "/dev/weact35"
#define CANONICAL "/dev/ttyACM0"
/* Private identity policy copied from the MIT diagnostic; no device I/O. */
typedef struct {
    char vid[16],pid[16],manufacturer[64],product[64],serial[64],driver[64];
    dev_t rdev;
    bool nodes_ok,complete,busy,occupancy_blocked;
    int error;
    char occupancy_operation[32],occupancy_path[PATH_MAX],holder_path[PATH_MAX];
    int occupancy_errno;
    uint64_t deadline_ms;
} identity_t;
static bool guard_expired(const identity_t *id)
{
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)<0) return true;
    if(ts.tv_sec<0 || (uint64_t)ts.tv_sec>UINT64_MAX/1000) {errno=EOVERFLOW;return true;}
    uint64_t now=(uint64_t)ts.tv_sec*1000+(uint64_t)ts.tv_nsec/1000000;
    if(now>=id->deadline_ms) {errno=ETIMEDOUT;return true;}
    return false;
}
/* Read metadata files only; never open a tty here. Reject truncated values. */
static int text_file(const char *path,char *out,size_t capacity)
{
    FILE *f=fopen(path,"r");if(!f) return -1;
    size_t n=fread(out,1,capacity-1,f);int error=ferror(f)?EIO:0;
    if(!error && !feof(f)) {int extra=fgetc(f);if(extra!=EOF) error=EOVERFLOW;else if(ferror(f)) error=EIO;}
    if(memchr(out,0,n)) error=EINVAL;
    if(fclose(f)<0 && !error) error=errno;
    if(error) {errno=error;return -1;}
    while(n && (out[n-1]=='\n' || out[n-1]=='\r')) --n;
    out[n]='\0';return 0;
}
static int attribute(const char *base,const char *name,char *out,size_t capacity)
{
    char path[PATH_MAX];int n=snprintf(path,sizeof(path),"%s/%s",base,name);
    if(n<0 || (size_t)n>=sizeof(path)) {errno=ENAMETOOLONG;return -1;}
    return text_file(path,out,capacity);
}
static bool numeric(const char *s)
{
    if(!*s) return false;
    for(;*s;++s) if(!isdigit((unsigned char)*s)) return false;
    return true;
}
/* Preserve the first failure before cleanup can overwrite errno.
 * Permission-only UNKNOWN can be accepted only by an explicit policy. */
static int occupancy_failure(identity_t *id,const char *operation,const char *path,int error)
{
    id->complete=false;
    long pid=0;
    bool foreign_permission=(error==EACCES || error==EPERM) &&
        (!strcmp(operation,"opendir_fd") || !strcmp(operation,"stat_fd") || !strcmp(operation,"readdir_fd")) &&
        sscanf(path,"/proc/%ld/",&pid)==1 && pid>0 && pid!=(long)getpid();
    if(!foreign_permission) id->occupancy_blocked=true;
    if(!id->occupancy_errno) {
        id->occupancy_errno=error;
        snprintf(id->occupancy_operation,sizeof(id->occupancy_operation),"%s",operation);
        snprintf(id->occupancy_path,sizeof(id->occupancy_path),"%s",path);
    }
    errno=id->occupancy_errno;return -1;
}
/* No claims about hidden host namespaces. A non-host-like pid 1 or any
 * permission failure is UNKNOWN; continue scanning accessible processes. */
static int users_of_port(identity_t *id,int exclude_fd)
{
    char init[64];
    if(text_file("/proc/1/comm",init,sizeof(init))<0)
        return occupancy_failure(id,"read_comm","/proc/1/comm",errno);
    if(strcmp(init,"systemd") && strcmp(init,"init"))
        return occupancy_failure(id,"namespace_guard","/proc/1/comm",EPERM);
    DIR *proc=opendir("/proc");
    if(!proc) return occupancy_failure(id,"opendir_proc","/proc",errno);
    id->complete=true;id->busy=false;struct dirent *process;
    for(;;) {
        if(guard_expired(id)) {(void)occupancy_failure(id,"deadline","/proc",errno);break;}
        errno=0;process=readdir(proc);
        if(!process) {if(errno) (void)occupancy_failure(id,"readdir_proc","/proc",errno);break;}
        if(!numeric(process->d_name)) continue;
        char path[PATH_MAX];
        int n=snprintf(path,sizeof(path),"/proc/%s/fd",process->d_name);
        if(n<0 || (size_t)n>=sizeof(path)) {(void)occupancy_failure(id,"format_fd_path","/proc",ENAMETOOLONG);break;}
        DIR *fds=opendir(path);
        if(!fds) {
            int directory_error=errno;
            if(directory_error==ENOENT || directory_error==ESRCH) {
                char process_path[PATH_MAX];struct stat exists;
                int written=snprintf(process_path,sizeof(process_path),"/proc/%s",process->d_name);
                if(written<0 || (size_t)written>=sizeof(process_path)) {
                    (void)occupancy_failure(id,"format_pid_path","/proc",ENAMETOOLONG);break;
                }
                if(stat(process_path,&exists)<0) {
                    if(errno==ENOENT || errno==ESRCH) continue;
                    (void)occupancy_failure(id,"stat_process",process_path,errno);break;
                }
            }
            (void)occupancy_failure(id,"opendir_fd",path,directory_error);continue;
        }
        struct dirent *fd;
        for(;;) {
            if(guard_expired(id)) {(void)occupancy_failure(id,"deadline",path,errno);break;}
            errno=0;fd=readdir(fds);
            if(!fd) {if(errno) (void)occupancy_failure(id,"readdir_fd",path,errno);break;}
            if(!numeric(fd->d_name)) continue;
            if(strtol(process->d_name,NULL,10)==(long)getpid() && strtol(fd->d_name,NULL,10)==exclude_fd) continue;
            char entry[PATH_MAX];n=snprintf(entry,sizeof(entry),"%s/%s",path,fd->d_name);
            if(n<0 || (size_t)n>=sizeof(entry)) {(void)occupancy_failure(id,"format_entry",path,ENAMETOOLONG);break;}
            struct stat st;
            if(stat(entry,&st)<0) {
                if(errno==ENOENT || errno==ESRCH) continue;
                (void)occupancy_failure(id,"stat_fd",entry,errno);continue;
            }
            if(S_ISCHR(st.st_mode) && st.st_rdev==(id->rdev ? id->rdev : makedev(166,0))) {
                id->busy=true;snprintf(id->holder_path,sizeof(id->holder_path),"%s",entry);
            }
        }
        if(closedir(fds)<0) (void)occupancy_failure(id,"closedir_fd",path,errno);
        if(id->busy) break;
    }
    if(closedir(proc)<0) (void)occupancy_failure(id,"closedir_proc","/proc",errno);
    if(!id->complete) {errno=id->occupancy_errno;return -1;}
    return 0;
}
#ifndef WX_GUARD_OCCUPANCY_ONLY
static int collect_identity(void *context,identity_t *id,int exclude_fd,uint64_t end)
{
    (void)context;memset(id,0,sizeof(*id));id->deadline_ms=end;
    if(guard_expired(id)) return -1;
    char interface[PATH_MAX],usb[PATH_MAX],driver[PATH_MAX],alias[PATH_MAX],canonical[PATH_MAX];
    if(!realpath("/sys/class/tty/ttyACM0/device",interface)) return -1;
    strcpy(usb,interface);char *slash=strrchr(usb,'/');if(!slash) {errno=EINVAL;return -1;}*slash='\0';
    if(attribute(usb,"idVendor",id->vid,sizeof(id->vid))<0 ||
       attribute(usb,"idProduct",id->pid,sizeof(id->pid))<0 ||
       attribute(usb,"manufacturer",id->manufacturer,sizeof(id->manufacturer))<0 ||
       attribute(usb,"product",id->product,sizeof(id->product))<0 ||
       attribute(usb,"serial",id->serial,sizeof(id->serial))<0) return -1;
    char driver_link[PATH_MAX];int n=snprintf(driver_link,sizeof(driver_link),"%s/driver",interface);
    if(n<0 || (size_t)n>=sizeof(driver_link)) {errno=ENAMETOOLONG;return -1;}
    if(!realpath(driver_link,driver)) return -1;
    if(strcmp(driver,"/sys/bus/usb/drivers/cdc_acm")) {errno=ENODEV;return -1;}
    strcpy(id->driver,"cdc_acm");
    char number[16],cls[16],sub[16],proto[16],dev[32];
    if(attribute(interface,"bInterfaceNumber",number,sizeof(number))<0 ||
       attribute(interface,"bInterfaceClass",cls,sizeof(cls))<0 ||
       attribute(interface,"bInterfaceSubClass",sub,sizeof(sub))<0 ||
       attribute(interface,"bInterfaceProtocol",proto,sizeof(proto))<0 ||
       text_file("/sys/class/tty/ttyACM0/dev",dev,sizeof(dev))<0) return -1;
    if(strcmp(number,"00") || strcmp(cls,"02") || strcmp(sub,"02") || strcmp(proto,"01") || strcmp(dev,"166:0")) {errno=ENODEV;return -1;}
    struct stat a,b;
    if(stat(TARGET,&a)<0 || stat(CANONICAL,&b)<0 ||
       !realpath(TARGET,alias) || !realpath(CANONICAL,canonical)) {id->error=errno;return 0;}
    id->nodes_ok=S_ISCHR(a.st_mode) && S_ISCHR(b.st_mode) &&
        a.st_rdev==b.st_rdev && major(a.st_rdev)==166 && minor(a.st_rdev)==0 &&
        !strcmp(alias,CANONICAL) && !strcmp(canonical,CANONICAL);
    if(!id->nodes_ok) {id->error=ENODEV;return 0;}
    if(users_of_port(id,exclude_fd)<0) {id->error=errno;id->complete=false;}
    return 0;
}
static bool fs35_identity_matches(const identity_t *id)
{
    return !strcmp(id->vid,"1a86") && !strcmp(id->pid,"fe0c") &&
        !strcmp(id->manufacturer,"WeAct Studio") && !strcmp(id->product,"Display FS V1") &&
        !strcmp(id->driver,"cdc_acm");
}

#endif /* guarded FS35 collector; occupancy-only consumers do not use it */
#endif
