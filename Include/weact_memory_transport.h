/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#ifndef WEACT_MEMORY_TRANSPORT_H
#define WEACT_MEMORY_TRANSPORT_H
#include "weact_extended.h"

/* Deterministic artificial transport with caller-owned storage and script.
 * No file descriptors, device paths, allocations or system clock calls.
 * WX_OK script entry with limit=0 deliberately returns zero progress.
 * Exhausted script -> normal writes, bounded by write_limit when nonzero. */
typedef struct { wx_status_t status; size_t limit; } wx_memory_action_t;
typedef struct {
    uint8_t *bytes;
    size_t capacity, length, write_limit, writes, waits;
    const wx_memory_action_t *script;
    size_t script_length, script_index;
    uint64_t time_ms, tick_ms, wait_ms;
    wx_status_t wait_status;
    bool disconnected;
} wx_memory_transport_t;

wx_status_t wx_memory_transport_init(wx_memory_transport_t *memory,
                                     uint8_t *storage, size_t capacity);
wx_transport_t wx_memory_transport_bind(wx_memory_transport_t *memory);
#endif
