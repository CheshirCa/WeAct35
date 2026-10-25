/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest */
#ifndef WEACT_GRAPHICS_H
#define WEACT_GRAPHICS_H
#include "weact_extended.h"
/* Model/geometry reflect the selected profile, never device autodetection. */
wx_status_t wx_device_profile(const wx_device_t *, wx_model_t *, wx_surface_info_t *);
/* Strict bounds, no clipping. Empty rectangles at an in-bounds edge are no-ops.
 * RGB888 is profile-independent. No I/O. */
wx_status_t wx_surface_fill_rgb(wx_surface_t *, size_t x, size_t y,
                              size_t width, size_t height, uint8_t r, uint8_t g, uint8_t b);
/* Copies RGB888 rows; caller retains storage, needed only during call.
 * stride=0 means width*3. Source must not overlap surface storage.
 * length must cover height*stride, including padding. */
wx_status_t wx_surface_bitmap_rgb(wx_surface_t *, size_t x, size_t y,
                                size_t width, size_t height, const uint8_t *,
                                size_t length, size_t stride);
/* Send a nonempty region of the device canvas with existing Bitmap 05.
 * Strict bounds; source lifetime/serialization and fail-stop match full Bitmap.
 * No host mutation, allocation, background readback or implicit orientation.
 * Managed devices require confirmed geometry, including preserve sessions. */
wx_status_t wx_device_bitmap_region(wx_device_t *,size_t x,size_t y,size_t width,size_t height);
#endif
