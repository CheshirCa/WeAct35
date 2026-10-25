/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools; based on CheshirCa / WeAct Display Tools Contributors.
 * See LICENSE (MIT).
 */
#ifndef WEACT_POSIX_TRANSPORT_H
#define WEACT_POSIX_TRANSPORT_H
#include "weact_extended.h"

typedef struct wx_posix_transport wx_posix_transport_t;
typedef struct {
    size_t struct_size;
    bool rtscts;           /* explicit opt-in; default false */
    bool hangup_on_close;  /* HUPCL, default false; does not control open-time DTR */
    wx_allocator_t allocator;
} wx_posix_options_t;

/* Linux tty backend: O_NONBLOCK|O_NOCTTY|O_CLOEXEC, raw 8N1, nominal B115200.
 * No protocol bytes, tcflush, drain, reset or modem-line ioctl. Kernel/driver
 * may change modem lines on open/close; opening hardware needs authorization.
 * On failure *out=NULL, all resources released, errno is the primary error.
 * Settings are not restored on close. One owner, no concurrent close/use.
 * Keep the backend alive until all devices borrowing its callbacks are destroyed.
 * Transaction deadline/sealing belongs to wx_device_t. Do not share a backend
 * across devices, or recreate a failed device without external verified recovery.
 */
wx_status_t wx_posix_transport_open(const char *path, const wx_posix_options_t *options,
                                   wx_posix_transport_t **out);
wx_transport_t wx_posix_transport_bind(wx_posix_transport_t *transport);
/* Close once, no flush/drain, no retry of close(EINTR) on Linux; idempotent.
 * An error still invalidates the fd; destroy will not close it again. */
wx_status_t wx_posix_transport_close(wx_posix_transport_t *transport);
void wx_posix_transport_destroy(wx_posix_transport_t *transport);
int wx_posix_transport_last_errno(const wx_posix_transport_t *transport);
#endif
