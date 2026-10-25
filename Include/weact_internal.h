/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#ifndef WEACT_INTERNAL_H
#define WEACT_INTERNAL_H
#include "weact_extended.h"
#include "weact_init.h"

struct wx_surface {
    wx_surface_info_t info;
    wx_allocator_t allocator;
    uint8_t *pixels;
};
typedef struct wx_init_metadata wx_init_metadata_t;
struct wx_device {
    wx_model_t model;
    uint8_t orientation;
    wx_surface_t *surface;
    wx_allocator_t allocator;
    wx_transport_t transport;
    uint64_t timeout_ms;
    bool failed;
    wx_init_metadata_t *init; /* NULL for every old create/borrowed object. */
};
struct wx_init_metadata {
    wx_init_info_t info;
    wx_init_channel_t channel;
    wx_init_report_t report;
    bool owned, sealed;
};
/* Private hooks; never acquire or emit protocol bytes. */
wx_status_t wx_init_dispose_channel(wx_device_t *device, bool failed);
void wx_init_metadata_destroy(wx_device_t *device);

bool wx_allocator_valid(const wx_allocator_t *allocator);
void *wx_allocate(const wx_allocator_t *allocator, size_t size);
void wx_release(const wx_allocator_t *allocator, void *pointer);
#endif
