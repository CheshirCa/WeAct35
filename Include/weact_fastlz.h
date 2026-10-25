/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Independent bounded FastLZ level-1 encoder. See LICENSE (MIT).
 */
#ifndef WEACT_FASTLZ_H
#define WEACT_FASTLZ_H
#include "weact_extended.h"
#define WX_FASTLZ_BLOCK_SIZE 1920u
/* Capacity n + ceil(n/32) suffices for this encoder. Nonoverlapping buffers.
 * Only FS35 landscape blocks (1..1920 bytes); no allocations or CPU intrinsics. */
wx_status_t wx_fastlz_encode(const uint8_t *,size_t,uint8_t *,size_t,size_t *);
#endif
