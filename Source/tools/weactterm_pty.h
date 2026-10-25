/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 */
#ifndef WEACTTERM_PTY_H
#define WEACTTERM_PTY_H
#include <stdbool.h>
#include <sys/types.h>
typedef struct {int master,status,rows,cols;pid_t pid;bool reaped;} wt_pty;
int wt_pty_open(wt_pty *,const char *command);
int wt_pty_open_size(wt_pty *,const char *command,int rows,int cols);
int wt_pty_resize(wt_pty *);
int wt_pty_reap(wt_pty *);
void wt_pty_close(wt_pty *);
#endif
