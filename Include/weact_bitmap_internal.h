/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see LICENSE (MIT). Private managed API.
 */
#ifndef WEACT_BITMAP_INTERNAL_H
#define WEACT_BITMAP_INTERNAL_H
#include "weact_extended.h"
typedef struct {
    bool measure_time;
    size_t header_bytes,pixel_bytes,write_calls;
    uint64_t write_ns,wait_ns;
} wx_bitmap_stats_t;
/* FS35/2 owned, confirmed geometry only. Region is x,y,width,height.
 * Source is the device canvas; one deadline, separate header/payload,
 * exact existing encoder and fail-stop semantics. Optional stats report host
 * accepted bytes and syscall times, never LCD latency. Not a public ABI. */
wx_status_t wx_device_bitmap_rect_internal(wx_device_t *,size_t,size_t,size_t,size_t,
                                           wx_bitmap_stats_t *);
#endif
