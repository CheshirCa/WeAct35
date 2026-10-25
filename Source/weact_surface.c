/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#include "weact_internal.h"
#include <stdlib.h>
#include <string.h>

bool wx_allocator_valid(const wx_allocator_t *a)
{
    return !a || (!!a->allocate == !!a->release);
}
void *wx_allocate(const wx_allocator_t *a, size_t size)
{
    return a && a->allocate ? a->allocate(a->context, size) : malloc(size);
}
void wx_release(const wx_allocator_t *a, void *pointer)
{
    if (!pointer) return;
    if (a && a->release) a->release(a->context, pointer);
    else free(pointer);
}

wx_status_t wx_surface_create(size_t width, size_t height, size_t stride,
                              wx_pixel_format_t format,
                              const wx_allocator_t *allocator, wx_surface_t **out)
{
    if (!out) return WX_INVALID;
    *out = NULL;
    if (!width || !height || !wx_allocator_valid(allocator) ||
        (format != WX_LEGACY_BRG565_BE && format != WX_RGB565_LE)) return WX_INVALID;
    if (width > SIZE_MAX / 2) return WX_OVERFLOW;
    size_t row_bytes = width * 2;
    if (!stride) stride = row_bytes;
    if (stride < row_bytes) return WX_INVALID;
    if (height > SIZE_MAX / stride) return WX_OVERFLOW;
    size_t capacity = height * stride;
    wx_surface_t *s = wx_allocate(allocator, sizeof(*s));
    if (!s) return WX_NOMEM;
    memset(s, 0, sizeof(*s));
    if (allocator) s->allocator = *allocator;
    s->info = (wx_surface_info_t){width, height, stride, capacity, format};
    s->pixels = wx_allocate(&s->allocator, capacity);
    if (!s->pixels) {
        wx_release(&s->allocator, s);
        return WX_NOMEM;
    }
    memset(s->pixels, 0, capacity);
    *out = s;
    return WX_OK;
}
void wx_surface_destroy(wx_surface_t *s)
{
    if (!s) return;
    wx_allocator_t allocator = s->allocator;
    wx_release(&allocator, s->pixels);
    wx_release(&allocator, s);
}
wx_status_t wx_surface_info(const wx_surface_t *s, wx_surface_info_t *out)
{
    if (!s || !out) return WX_INVALID;
    *out = s->info;
    return WX_OK;
}
uint8_t *wx_surface_pixels(wx_surface_t *s, size_t *length)
{
    if (length) *length = s ? s->info.capacity : 0;
    return s ? s->pixels : NULL;
}
uint16_t wx_pack_rgb(wx_pixel_format_t format, uint8_t r, uint8_t g, uint8_t b)
{
    if (format == WX_LEGACY_BRG565_BE)
        return (uint16_t)(((b >> 3) << 11) | ((r >> 3) << 6) | (g >> 2));
    if (format == WX_RGB565_LE)
        return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return 0;
}
static void store_pixel(wx_surface_t *s, size_t offset, uint16_t color)
{
    if (s->info.format == WX_RGB565_LE) {
        s->pixels[offset] = (uint8_t)color;
        s->pixels[offset + 1] = (uint8_t)(color >> 8);
    } else {
        s->pixels[offset] = (uint8_t)(color >> 8);
        s->pixels[offset + 1] = (uint8_t)color;
    }
}
wx_status_t wx_surface_set_pixel(wx_surface_t *s, int x, int y, uint16_t color)
{
    if (!s) return WX_INVALID;
    if (x < 0 || y < 0 || (size_t)x >= s->info.width || (size_t)y >= s->info.height)
        return WX_BOUNDS;
    store_pixel(s, (size_t)y * s->info.stride + (size_t)x * 2, color);
    return WX_OK;
}
wx_status_t wx_surface_set_rgb(wx_surface_t *s, int x, int y,
                               uint8_t r, uint8_t g, uint8_t b)
{
    if (!s) return WX_INVALID;
    return wx_surface_set_pixel(s, x, y, wx_pack_rgb(s->info.format, r, g, b));
}
wx_status_t wx_surface_clear(wx_surface_t *s, uint16_t color)
{
    if (!s) return WX_INVALID;
    for (size_t y = 0; y < s->info.height; ++y)
        for (size_t x = 0; x < s->info.width; ++x)
            store_pixel(s, y * s->info.stride + x * 2, color);
    return WX_OK;
}
