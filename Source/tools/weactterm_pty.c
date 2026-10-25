/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "weactterm_pty.h"
#include "weactterm_screen.h"
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
int wt_pty_resize(wt_pty *p)
{
    struct winsize ws={.ws_row=(unsigned short)p->rows,.ws_col=(unsigned short)p->cols,.ws_xpixel=480,.ws_ypixel=320};
    return ioctl(p->master,TIOCSWINSZ,&ws);
}
int wt_pty_open(wt_pty *p,const char *command)
{
    return wt_pty_open_size(p,command,WT_ROWS,WT_COLS);
}
int wt_pty_open_size(wt_pty *p,const char *command,int rows,int cols)
{
    *p=(wt_pty){.master=-1,.pid=-1,.rows=rows,.cols=cols};
    if(rows<1 || rows>WT_MAX_ROWS || cols<1 || cols>WT_MAX_COLS){errno=EINVAL;return -1;}
    struct winsize ws={.ws_row=(unsigned short)p->rows,.ws_col=(unsigned short)p->cols,.ws_xpixel=480,.ws_ypixel=320};
    p->pid=forkpty(&p->master,NULL,NULL,&ws);
    if(p->pid<0)return -1;
    if(!p->pid) {
        const int signals[]={SIGINT,SIGTERM,SIGHUP,SIGCHLD,SIGWINCH,SIGPIPE};
        for(size_t n=0;n<sizeof(signals)/sizeof(signals[0]);++n)signal(signals[n],SIG_DFL);
        unsetenv("COLUMNS");unsetenv("LINES");
        setenv("TERM","xterm",1);setenv("LC_ALL","C.UTF-8",1);
        if(command)execl("/bin/sh","sh","-c",command,(char *)NULL);
        else {
            const char *shell=getenv("SHELL");if(!shell || shell[0]!='/')shell="/bin/sh";
            execl(shell,shell,"-i",(char *)NULL);
        }
        _exit(127);
    }
    int flags=fcntl(p->master,F_GETFL);
    if(wt_pty_resize(p)<0 || flags<0 || fcntl(p->master,F_SETFL,flags|O_NONBLOCK)<0 || fcntl(p->master,F_SETFD,FD_CLOEXEC)<0) {
        wt_pty_close(p);return -1;
    }
    return 0;
}
int wt_pty_reap(wt_pty *p)
{
    if(p->reaped)return 1;
    pid_t result=waitpid(p->pid,&p->status,WNOHANG);
    if(result==p->pid){p->reaped=true;return 1;}
    return result<0 && errno!=EINTR?-1:0;
}
void wt_pty_close(wt_pty *p)
{
    if(p->master>=0) {
        pid_t foreground=0;
        if(ioctl(p->master,TIOCGPGRP,&foreground)==0 && foreground>0 && foreground!=getpgrp())kill(-foreground,SIGHUP);
        close(p->master);p->master=-1;
    }
    if(p->pid<=0 || p->reaped)return;
    kill(-p->pid,SIGHUP);kill(p->pid,SIGHUP);
    for(unsigned n=0;n<30;++n) {
        if(wt_pty_reap(p)!=0)return;
        struct timespec delay={.tv_nsec=10000000};nanosleep(&delay,NULL);
    }
    kill(-p->pid,SIGKILL);kill(p->pid,SIGKILL);
    while(waitpid(p->pid,&p->status,0)<0 && errno==EINTR){}
    p->reaped=true;
}
