/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Additive opt-in FS35 features. See LICENSE (MIT).
 */
#ifndef WEACT_FS35_H
#define WEACT_FS35_H
#include "weact_extended.h"
typedef struct {
    size_t struct_size;
    bool measure_time;
    size_t original_bytes,compressed_bytes,block_bytes,wire_bytes,blocks;
    uint64_t compression_ns,tx_ns,wait_ns,write_ns;
} wx_fastlz_stats_t;
/* Owned managed FS35/2, confirmed 480x320 RGB565 LE only. All compression and
 * allocation precede TX. Separate header and block writes, one TX deadline,
 * fail-stop, no fallback. Stats: planned original/compressed/block bytes;
 * wire_bytes is actually accepted TX, blocks counts complete transmitted blocks.
 * block_bytes includes four metadata bytes per block; wire adds 10-byte header.
 * No visual or firmware-support guarantee. Serialize calls per device. */
wx_status_t wx_device_bitmap_fastlz(wx_device_t *,wx_fastlz_stats_t *);
wx_status_t wx_device_bitmap_fastlz_rect(wx_device_t *,size_t,size_t,size_t,size_t,wx_fastlz_stats_t *);
/* Percent 0..100, floor(percent*255/100). Fade capped at 5000 ms, matching
 * manufacturer setting tool and existing raw wx_device_set_brightness API.
 * CLI policy is stricter (0..50%). Managed ready/preserved FS35 only.
 * Preserve never sends this automatically; does not change canvas pixels. */
wx_status_t wx_device_set_backlight_percent(wx_device_t *,unsigned,uint16_t);
/* Explicit readback: 83 0A -> 83 raw_byte 0A. Ready/preserved managed FS35,
 * guarded strict RX with one deadline; errors fail-stop. No automatic restore. */
wx_status_t wx_device_get_backlight(wx_device_t *,uint8_t *);
#endif
