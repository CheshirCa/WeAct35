/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see LICENSE (MIT).
 */
#ifndef WEACT_POSIX_INIT_H
#define WEACT_POSIX_INIT_H
#include "weact_init.h"
/* Guarded FS35 connector for a configured /dev/weact35 -> /dev/ttyACM0 mapping.
 * It accepts any matching FS35; no serial is pinned. Binding performs no I/O.
 * Options must outlive acquisition; no fd/path bypass.
 * Strict occupancy by default; explicit limited policy accepts only foreign
 * process permission failures, never namespace or scanner failures. */
typedef struct {
    size_t struct_size;
    bool allow_unknown_occupancy;
    wx_allocator_t allocator;
} wx_posix_init_options_t;
wx_status_t wx_posix_init_connector(const wx_posix_init_options_t *,wx_init_connector_t *);
/* Portable managed discovery; old options/layout and fixed connector above
 * remain compatible. NULL device/serial select the sole matching CDC ACM.
 * This options object and its strings must outlive device acquisition. */
typedef struct {
    size_t struct_size;
    wx_posix_init_options_t base;
    const char *device;
    const char *serial;
} wx_posix_select_options_t;
wx_status_t wx_posix_init_connector_select(const wx_posix_select_options_t *,wx_init_connector_t *);
#endif
