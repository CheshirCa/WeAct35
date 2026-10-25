/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT). SSH keyboard only.
 */
#ifndef WEACTTERM_INPUT_H
#define WEACTTERM_INPUT_H
#include "weactterm_screen.h"
typedef struct {
    char escape[32];size_t length;uint64_t escape_since;
    uint32_t code,min;unsigned needed;
    bool stop,ctrl_c_to_pty;
} wt_input;
void wt_input_bytes(wt_input *,wt_screen *,const char *,size_t,uint64_t);
void wt_input_tick(wt_input *,wt_screen *,uint64_t);
#endif
