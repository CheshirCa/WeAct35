/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; see LICENSE (MIT). Read-only USB inventory.
 */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool acm_name(const char *name)
{
    if(strncmp(name,"ttyACM",6) || !name[6])return false;
    for(const char *p=name+6;*p;p++)if(*p<'0' || *p>'9')return false;
    return true;
}
static int join(char out[PATH_MAX],const char *a,const char *b)
{
    int n=snprintf(out,PATH_MAX,"%s/%s",a,b);
    if(n<0 || n>=PATH_MAX){errno=ENAMETOOLONG;return -1;}
    return 0;
}
/* Same sysfs tty->interface->USB parent traversal as weact_linux_select.h.
 * A short bounded reader is local because inventory allows missing optional
 * fields and UNKNOWN products; guarded FS35 selection has a stricter contract.
 * Only this reader opens files, and callers pass USB sysfs attribute paths.
 */
static int attribute(const char *dir,const char *name,char out[256])
{
    char path[PATH_MAX];out[0]=0;
    if(join(path,dir,name)<0)return -1;
    int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;
    char bytes[256];size_t used=0;int failure=0;
    while(used<sizeof(bytes)) {
        ssize_t n=read(fd,bytes+used,sizeof(bytes)-used);
        if(n<0){if(errno==EINTR)continue;failure=errno;break;}
        if(!n)break;
        used+=(size_t)n;
    }
    if(close(fd)<0 && !failure)failure=errno;
    if(!failure && used==sizeof(bytes))failure=EOVERFLOW;
    if(failure){errno=failure;return -1;}
    if(used && bytes[used-1]=='\n')used--;
    for(size_t i=0;i<used;i++) {
        unsigned char c=(unsigned char)bytes[i];out[i]=(c<32 || c==127)?'?':bytes[i];
    }
    out[used]=0;return 0;
}
static void clean_text(char *text)
{
    for(unsigned char *p=(unsigned char *)text;*p;p++)if(*p<32 || *p==127)*p='?';
}
static void warning(const char *tty,const char *field,int error)
{fprintf(stderr,"weact-list: %s: %s: %s\n",tty,field,strerror(error));}
static int optional(const char *usb,const char *name,char out[256],const char *tty)
{
    if(attribute(usb,name,out)==0)return 0;
    if(errno==ENOENT)return 0;
    warning(tty,name,errno);return 1;
}
static void free_names(struct dirent **names,int count)
{for(int i=0;i<count;i++)free(names[i]);free(names);}
/* Resolve links only. Never open /dev entries, including serial ports. */
static int aliases(const char *dev,const char *tty,bool verbose)
{
    char directory[PATH_MAX],node[PATH_MAX],canonical[PATH_MAX];
    if(join(directory,dev,"serial/by-id")<0 || join(node,dev,tty)<0)return 1;
    if(!realpath(node,canonical)) {
        if(verbose)warning(tty,"device node missing/stale",errno);
        fputs("-",stdout);return 0;
    }
    struct dirent **names=NULL;int count=scandir(directory,&names,NULL,alphasort);
    if(count<0) {
        int error=errno;fputs("-",stdout);
        if(error==ENOENT)return 0;
        warning(tty,"serial/by-id",error);return 1;
    }
    bool printed=false;int failed=0;
    for(int i=0;i<count;i++) {
        if(names[i]->d_name[0]=='.')continue;
        char path[PATH_MAX],target[PATH_MAX];
        if(join(path,directory,names[i]->d_name)<0){failed=1;continue;}
        if(!realpath(path,target)) {
            if(verbose)warning(tty,"stale/unreadable by-id link",errno);
            continue;
        }
        if(strcmp(canonical,target))continue;
        if(printed)putchar(',');
        clean_text(path);fputs(path,stdout);printed=true;
    }
    if(!printed)putchar('-');
    free_names(names,count);return failed;
}
static int list_at(const char *root,const char *dev,bool verbose)
{
    struct dirent **names=NULL;int count=scandir(root,&names,NULL,alphasort);
    if(count<0){warning(root,"cannot scan sysfs",errno);return 1;}
    puts("MODEL\tVID:PID\tPRODUCT\tMANUFACTURER\tSERIAL\tTTY\tBY-ID");
    size_t found=0;int failed=0;
    for(int k=0;k<count;k++) {
        const char *tty=names[k]->d_name;if(!acm_name(tty))continue;
        char path[PATH_MAX],entry[PATH_MAX],iface[PATH_MAX],usb[PATH_MAX];
        if(join(entry,root,tty)<0 || join(path,entry,"device")<0){failed=1;continue;}
        if(!realpath(path,iface)) {
            if(verbose)warning(tty,"stale sysfs link",errno);
            continue;
        }
        memcpy(usb,iface,strlen(iface)+1);char *slash=strrchr(usb,'/');
        if(!slash){failed=1;continue;}*slash=0;
        char vid[256],pid[256],product[256],manufacturer[256],serial[256];
        if(attribute(usb,"idVendor",vid)<0) {
            if(errno!=ENOENT){warning(tty,"idVendor",errno);failed=1;}
            continue;
        }
        if(strcmp(vid,"1a86"))continue;
        if(attribute(usb,"idProduct",pid)<0){warning(tty,"idProduct",errno);failed=1;continue;}
        if(strcmp(pid,"fe0c"))continue;
        if(attribute(usb,"product",product)<0){warning(tty,"product",errno);failed=1;}
        failed|=optional(usb,"manufacturer",manufacturer,tty);
        failed|=optional(usb,"serial",serial,tty);
        const char *model=!strcmp(product,"Display FS V1")?"FS35":
                          !strcmp(product,"Display FS 0.96 Inch")?"FS096":"UNKNOWN";
        printf("%s\t%s:%s\t%s\t%s\t%s\t%s/%s\t",model,vid,pid,
               *product?product:"-",*manufacturer?manufacturer:"-",*serial?serial:"-",dev,tty);
        failed|=aliases(dev,tty,verbose);putchar('\n');found++;
        if(verbose) {
            char number[256],cls[256],sub[256],proto[256],driver[PATH_MAX];
            failed|=optional(iface,"bInterfaceNumber",number,tty);
            failed|=optional(iface,"bInterfaceClass",cls,tty);
            failed|=optional(iface,"bInterfaceSubClass",sub,tty);
            failed|=optional(iface,"bInterfaceProtocol",proto,tty);
            if(join(path,iface,"driver")<0 || !realpath(path,driver))strcpy(driver,"-");
            clean_text(entry);clean_text(iface);clean_text(usb);clean_text(driver);
            printf("  sysfs=%s\n  usb=%s\n  interface=%s number=%s class=%s subclass=%s protocol=%s driver=%s\n",
                   entry,usb,iface,*number?number:"-",*cls?cls:"-",*sub?sub:"-",*proto?proto:"-",driver);
        }
    }
    free_names(names,count);
    if(!found)puts("No WeAct USB displays found.");
    return failed || ferror(stdout)?1:0;
}
#ifndef WEACT_LIST_TEST
int main(int argc,char **argv)
{
    bool verbose=false;
    if(argc==2 && !strcmp(argv[1],"--help")) {
        puts("Usage: weact-list [--verbose|--help]\n"
             "List FS35, FS096 and UNKNOWN 1a86:fe0c USB devices by sysfs metadata.\n"
             "Read-only: never opens serial ports or sends display commands.\n"
             "Serial and manufacturer are optional; model is selected by Product.\n"
             "Exit: 0 success (including no devices), 1 incomplete scan, 2 invalid arguments.");
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--verbose"))verbose=true;
    else if(argc!=1){fputs("Usage: weact-list [--verbose|--help]\n",stderr);return 2;}
    return list_at("/sys/class/tty","/dev",verbose);
}
#endif
