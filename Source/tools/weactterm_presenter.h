/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT). One pending frame.
 */
#ifndef WEACTTERM_PRESENTER_H
#define WEACTTERM_PRESENTER_H
#include "weact_extended.h"
#include <pthread.h>
typedef struct {
    pthread_t thread;pthread_mutex_t lock;pthread_cond_t ready;
    wx_device_t *device;uint8_t *pending,*last;
    bool baseline_valid,partial,perf_enabled,fastlz;
    size_t original_bytes,compressed_bytes,compressed_block_bytes,wire_bytes,compressed_regions,compressed_frames;
    uint64_t compression_ns,transfer_ns;
    size_t processed,skipped,regions,full_frames,pixel_bytes,header_bytes;
    size_t attempted,full_baseline,full_borrowed,full_cost,region_merges;
    uint64_t pending_since,queue_wait_ns,bitmap_ns,tx_wait_ns,tx_write_ns,copy_ns,diff_ns;
    bool running,stop,available;wx_status_t status;size_t sent;
} wt_presenter;
int wt_presenter_open(wt_presenter *,wx_device_t *,bool);
int wt_presenter_open_mode(wt_presenter *,wx_device_t *,bool,bool);
int wt_presenter_submit(wt_presenter *,wx_surface_t *);
wx_status_t wt_presenter_status(wt_presenter *);
wx_status_t wt_presenter_close(wt_presenter *);
#endif
