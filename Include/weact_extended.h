/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools, based on work by CheshirCa and the Linux/C port
 * by WeAct Display Tools Contributors. See LICENSE (MIT).
 */
#ifndef WEACT_EXTENDED_H
#define WEACT_EXTENDED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WX_OK = 0, WX_INVALID, WX_OVERFLOW, WX_NOMEM, WX_BOUNDS,
    WX_INTERRUPTED, WX_AGAIN, WX_TIMEOUT, WX_DISCONNECTED, WX_IO,
    WX_STREAM_FAILED
} wx_status_t;
typedef enum { WX_FS096 = 1, WX_FS35 = 2 } wx_model_t;
typedef enum { WX_LEGACY_BRG565_BE = 1, WX_RGB565_LE = 2 } wx_pixel_format_t;
typedef struct wx_device wx_device_t;
typedef struct wx_surface wx_surface_t;

/* Both callbacks must be supplied, or both NULL for malloc/free.
 * Context must outlive allocated objects. Zero sizes are never requested. */
typedef struct {
    void *context;
    void *(*allocate)(void *context, size_t size);
    void (*release)(void *context, void *pointer);
} wx_allocator_t;

/* Injected nonblocking transport; no serial backend in stage 2A.
 * write: OK means 0..length bytes accepted; errors must report written=0.
 * now_ms is monotonic. wait_writable must return by absolute deadline and
 * make time/progress, including on spurious readiness. Callbacks must not
 * reenter the device. Context remains owned by the caller. */
typedef struct {
    void *context;
    wx_status_t (*write)(void *context, const uint8_t *bytes, size_t length,
                         size_t *written);
    uint64_t (*now_ms)(void *context);
    wx_status_t (*wait_writable)(void *context, uint64_t deadline_ms);
} wx_transport_t;

typedef struct {
    size_t width, height, stride, capacity;
    wx_pixel_format_t format;
} wx_surface_info_t;

/* Independent owned surface. stride=0 selects width*2; padding is not sent.
 * Explicit sizes prevent overflow before allocation. Allocator may be NULL. */
wx_status_t wx_surface_create(size_t width, size_t height, size_t stride,
                              wx_pixel_format_t format,
                              const wx_allocator_t *allocator,
                              wx_surface_t **out);
void wx_surface_destroy(wx_surface_t *surface);
wx_status_t wx_surface_info(const wx_surface_t *surface, wx_surface_info_t *out);
/* Borrowed storage; valid until destroy or successful orientation change.
 * length includes row padding. */
uint8_t *wx_surface_pixels(wx_surface_t *surface, size_t *length);
wx_status_t wx_surface_set_pixel(wx_surface_t *surface, int x, int y,
                                 uint16_t packed_color);
wx_status_t wx_surface_set_rgb(wx_surface_t *surface, int x, int y,
                               uint8_t r, uint8_t g, uint8_t b);
wx_status_t wx_surface_clear(wx_surface_t *surface, uint16_t packed_color);
uint16_t wx_pack_rgb(wx_pixel_format_t format, uint8_t r, uint8_t g, uint8_t b);

typedef struct {
    size_t struct_size;
    wx_model_t model;                 /* required: never autodetected */
    size_t width, height, stride;     /* explicit geometry, optional stride */
    uint8_t orientation;             /* 0/1 portrait; 2/3 landscape */
    uint64_t timeout_ms;              /* required, >0; one deadline/command */
    wx_transport_t transport;
    wx_allocator_t allocator;
} wx_device_options_t;

/* Pure host creation: no commands, no device opening; transport is borrowed.
 * struct_size must equal sizeof(options) for this API version.
 * FS35 RGB565 LE is based on manufacturer sources, NOT hardware verified.
 * No reset, auto-rotation, partial update, presenter or serial backend here. */
wx_status_t wx_device_create(const wx_device_options_t *options, wx_device_t **out);
void wx_device_destroy(wx_device_t *device);
wx_surface_t *wx_device_surface(wx_device_t *device); /* borrowed; do not destroy */
wx_status_t wx_device_set_orientation(wx_device_t *device, uint8_t orientation);
wx_status_t wx_device_set_brightness(wx_device_t *device, uint8_t brightness,
                                    uint16_t time_ms);
/* Fill sends only the hardware command; it does not modify host pixels. */
wx_status_t wx_device_fill(wx_device_t *device, uint16_t packed_color);
/* Always a complete frame; no swap or partial updates. */
wx_status_t wx_device_bitmap(wx_device_t *device);
bool wx_device_stream_failed(const wx_device_t *device);
/* Any transfer failure seals this stream: no retry header into unknown payload.
 * Recreate only after external, independently verified stream recovery.
 * One caller must serialize commands and pixel mutation during transmission. */

#endif
