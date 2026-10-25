/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#ifndef WEACT_PROTOCOL_H
#define WEACT_PROTOCOL_H
#include "weact_extended.h"
/* Pure encoders: no transport, allocation or device state. Inclusive ends.
 * Bounds encode packet fields only; not a partial-screen update API. */
wx_status_t wx_encode_bitmap(uint8_t *out, size_t capacity,
                             uint32_t xs, uint32_t ys, uint32_t xe, uint32_t ye);
/* Opt-in FS35 FastLZ header; same inclusive LE geometry as 05. */
wx_status_t wx_encode_bitmap_fastlz(uint8_t *,size_t,uint32_t,uint32_t,uint32_t,uint32_t);
wx_status_t wx_encode_fill(uint8_t *out, size_t capacity,
                           uint32_t xs, uint32_t ys, uint32_t xe, uint32_t ye,
                           uint16_t packed_color);
wx_status_t wx_encode_orientation(uint8_t *out, size_t capacity, uint8_t orientation);
wx_status_t wx_encode_brightness(uint8_t *out, size_t capacity,
                                 uint8_t brightness, uint16_t time_ms);
#endif
