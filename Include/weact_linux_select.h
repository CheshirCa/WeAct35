#ifndef WEACT_LINUX_SELECT_H
#define WEACT_LINUX_SELECT_H
/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see LICENSE (MIT).
 * Private FS35 sysfs selection, metadata only: never opens a device node.
 */
static bool portable_identity_matches(const identity_t *id,const char *serial)
{
    return fs35_identity_matches(id) && (!serial || !strcmp(id->serial,serial));
}
static bool acm_name(const char *name)
{return !strncmp(name,"ttyACM",6) && numeric(name+6);}
static int selection_metadata(const char *root,const char *name,identity_t *id,char *node,size_t capacity,uint64_t end)
{
    char path[PATH_MAX],interface[PATH_MAX],usb[PATH_MAX],driver[PATH_MAX],dev[32];
    memset(id,0,sizeof(*id));id->deadline_ms=end;
    if(guard_expired(id))return -1;
    int n=snprintf(path,sizeof(path),"%s/%s/device",root,name);
    if(n<0 || (size_t)n>=sizeof(path) || !realpath(path,interface))return -1;
    snprintf(usb,sizeof(usb),"%s",interface);char *slash=strrchr(usb,'/');
    if(!slash){errno=EINVAL;return -1;}*slash=0;
    if(attribute(usb,"idVendor",id->vid,sizeof(id->vid))<0 ||
       attribute(usb,"idProduct",id->pid,sizeof(id->pid))<0)return -1;
    /* An unrelated ACM device need not implement the rest of the FS35 metadata. */
    if(strcmp(id->vid,"1a86") || strcmp(id->pid,"fe0c"))return 0;
    if(attribute(usb,"manufacturer",id->manufacturer,sizeof(id->manufacturer))<0 ||
       attribute(usb,"product",id->product,sizeof(id->product))<0)return -1;
    if(attribute(usb,"serial",id->serial,sizeof(id->serial))<0 && errno!=ENOENT)return -1;
    n=snprintf(path,sizeof(path),"%s/driver",interface);
    if(n<0 || (size_t)n>=sizeof(path) || !realpath(path,driver))return -1;
    if(strcmp(driver,"/sys/bus/usb/drivers/cdc_acm")){errno=ENODEV;return -1;}
    strcpy(id->driver,"cdc_acm");
    char number[16],cls[16],sub[16],proto[16];
    if(attribute(interface,"bInterfaceNumber",number,sizeof(number))<0 ||
       attribute(interface,"bInterfaceClass",cls,sizeof(cls))<0 ||
       attribute(interface,"bInterfaceSubClass",sub,sizeof(sub))<0 ||
       attribute(interface,"bInterfaceProtocol",proto,sizeof(proto))<0)return -1;
    if(strcmp(number,"00") || strcmp(cls,"02") || strcmp(sub,"02") || strcmp(proto,"01")){errno=ENODEV;return -1;}
    n=snprintf(path,sizeof(path),"%s/%s/dev",root,name);
    if(n<0 || (size_t)n>=sizeof(path) || text_file(path,dev,sizeof(dev))<0)return -1;
    unsigned ma,mi;char extra;
    if(sscanf(dev,"%u:%u%c",&ma,&mi,&extra)!=2 || ma!=166){errno=ENODEV;return -1;}
    id->rdev=makedev(ma,mi);
    if(major(id->rdev)!=ma || minor(id->rdev)!=mi){errno=EOVERFLOW;return -1;}
    n=snprintf(node,capacity,"/dev/%s",name);
    if(n<0 || (size_t)n>=capacity){errno=ENAMETOOLONG;return -1;}
    struct stat st;if(stat(node,&st)<0)return -1;
    id->nodes_ok=S_ISCHR(st.st_mode) && st.st_rdev==id->rdev;
    if(!id->nodes_ok){errno=ENODEV;return -1;}
    return 0;
}
#ifndef WX_SELECTION_METADATA
#define WX_SELECTION_METADATA selection_metadata
#endif
/* root is private test injection; production always uses /sys/class/tty. */
static int select_device_at(const char *root,const char *device,const char *serial,
                            identity_t *out,char *node,size_t capacity,uint64_t end)
{
    if(device) {
        char canonical[PATH_MAX];struct stat st;
        if(!realpath(device,canonical) || stat(device,&st)<0 || !S_ISCHR(st.st_mode))return -1;
        const char *name=strrchr(canonical,'/');
        if(!name || strncmp(canonical,"/dev/",5) || !acm_name(++name)){errno=EINVAL;return -1;}
        if(WX_SELECTION_METADATA(root,name,out,node,capacity,end)<0 || !out->nodes_ok ||
           !portable_identity_matches(out,serial) || st.st_rdev!=out->rdev){errno=ENODEV;return -1;}
        return 0;
    }
    DIR *dir=opendir(root);if(!dir)return -1;
    unsigned matches=0;int error=0;struct dirent *entry;
    for(;;) {
        errno=0;entry=readdir(dir);if(!entry){error=errno;break;}
        if(!acm_name(entry->d_name))continue;
        identity_t id;char candidate[PATH_MAX];
        if(WX_SELECTION_METADATA(root,entry->d_name,&id,candidate,sizeof(candidate),end)<0){error=errno;break;}
        if(!portable_identity_matches(&id,serial))continue;
        if(++matches>1){error=ENOTUNIQ;break;}
        *out=id;
        if(strlen(candidate)>=capacity){error=ENAMETOOLONG;break;}
        strcpy(node,candidate);
    }
    if(closedir(dir)<0 && !error)error=errno;
    if(!error && matches!=1)error=ENODEV;
    if(error){errno=error;return -1;}return 0;
}
static int collect_selected_identity(const char *node,const char *serial,identity_t *id,int exclude_fd,uint64_t end)
{
    char selected[PATH_MAX];
    if(select_device_at("/sys/class/tty",node,serial,id,selected,sizeof(selected),end)<0)return -1;
    if(users_of_port(id,exclude_fd)<0){id->error=errno;id->complete=false;}
    return 0;
}
#endif
