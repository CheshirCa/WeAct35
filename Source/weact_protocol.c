/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#include "weact_protocol.h"

static void le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}
static wx_status_t bounds(uint8_t *out, size_t capacity, size_t needed,
                          uint32_t xs, uint32_t ys, uint32_t xe, uint32_t ye)
{
    if (!out || capacity < needed) return WX_INVALID;
    if (xs > xe || ys > ye || xe > UINT16_MAX || ye > UINT16_MAX) return WX_BOUNDS;
    le16(out + 1, (uint16_t)xs);
    le16(out + 3, (uint16_t)ys);
    le16(out + 5, (uint16_t)xe);
    le16(out + 7, (uint16_t)ye);
    return WX_OK;
}
wx_status_t wx_encode_bitmap(uint8_t *out, size_t capacity,
                             uint32_t xs, uint32_t ys, uint32_t xe, uint32_t ye)
{
    wx_status_t status = bounds(out, capacity, 10, xs, ys, xe, ye);
    if (status != WX_OK) return status;
    out[0] = 0x05;
    out[9] = 0x0a;
    return WX_OK;
}
wx_status_t wx_encode_fill(uint8_t *out, size_t capacity,
                           uint32_t xs, uint32_t ys, uint32_t xe, uint32_t ye,
                           uint16_t color)
{
    wx_status_t status = bounds(out, capacity, 12, xs, ys, xe, ye);
    if (status != WX_OK) return status;
    out[0] = 0x04;
    le16(out + 9, color); /* Both legacy FULL and FS35 use LE here. */
    out[11] = 0x0a;
    return WX_OK;
}
wx_status_t wx_encode_orientation(uint8_t *out, size_t capacity, uint8_t orientation)
{
    if (!out || capacity < 3 || (orientation > 3 && orientation != 5)) return WX_INVALID;
    out[0] = 0x02; out[1] = orientation; out[2] = 0x0a;
    return WX_OK;
}
wx_status_t wx_encode_brightness(uint8_t *out, size_t capacity,
                                 uint8_t brightness, uint16_t time_ms)
{
    if (!out || capacity < 5) return WX_INVALID;
    if (time_ms > 5000) time_ms = 5000;
    out[0] = 0x03; out[1] = brightness;
    le16(out + 2, time_ms); out[4] = 0x0a;
    return WX_OK;
}

wx_status_t wx_encode_bitmap_fastlz(uint8_t *out,size_t capacity,
                                     uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye)
{
    wx_status_t status=wx_encode_bitmap(out,capacity,xs,ys,xe,ye);
    if(status==WX_OK)out[0]=0x15;
    return status;
}
