/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see LICENSE (MIT).
 */
#ifndef WEACT_INIT_H
#define WEACT_INIT_H
#include "weact_extended.h"

/* Additive API: no changes to existing options, transport or status values. */
typedef enum {
    WX_INIT_CREATED, WX_INIT_CONNECTED, WX_INIT_ORIENTATION_CONFIRMED,
    WX_INIT_DISPLAY_CLEARED_TX, WX_INIT_READY, WX_INIT_READY_PRESERVED,
    WX_INIT_FAILED, WX_INIT_CLOSED
} wx_init_state_t;
typedef enum { WX_FB_UNKNOWN, WX_FB_BLACK_BASELINE_ASSUMED } wx_fb_sync_t;
typedef enum {
    WX_INIT_STAGE_VALIDATE, WX_INIT_STAGE_ALLOCATE, WX_INIT_STAGE_ACQUIRE,
    WX_INIT_STAGE_SET_ORIENTATION, WX_INIT_STAGE_QUERY_TX, WX_INIT_STAGE_RX,
    WX_INIT_STAGE_PRE_FILL, WX_INIT_STAGE_FILL_TX, WX_INIT_STAGE_COMMIT,
    WX_INIT_STAGE_COMPLETE, WX_INIT_STAGE_CLOSE
} wx_init_stage_t;
typedef enum {
    WX_INIT_REASON_NONE, WX_INIT_INVALID_OPTIONS, WX_INIT_NOT_IMPLEMENTED,
    WX_INIT_PROFILE_NOT_ENABLED, WX_INIT_ALLOCATION_FAILED,
    WX_INIT_ACQUIRE_FAILED, WX_INIT_INVALID_CHANNEL, WX_INIT_CLOCK_ERROR,
    WX_INIT_DEADLINE, WX_INIT_GEOMETRY_UNCONFIRMED, WX_INIT_CHANNEL_CLOSED,
    WX_INIT_RELEASE_FAILED, WX_INIT_IDENTITY_MISMATCH, WX_INIT_PORT_BUSY,
    WX_INIT_OCCUPANCY_UNKNOWN, WX_INIT_EXCLUSIVE_FAILED, WX_INIT_CONFIGURE_FAILED,
    WX_INIT_SHORT_WRITE, WX_INIT_ZERO_WRITE, WX_INIT_INVALID_RESPONSE,
    WX_INIT_UNEXPECTED_INPUT, WX_INIT_UNKNOWN_ORIENTATION,
    WX_INIT_ORIENTATION_MISMATCH, WX_INIT_RX_ZERO_OR_EOF, WX_INIT_TRANSPORT_ERROR
} wx_init_reason_t;
typedef enum { WX_INIT_OCCUPANCY_UNCHECKED, WX_INIT_OCCUPANCY_CLEAR_VISIBLE,
               WX_INIT_OCCUPANCY_BUSY, WX_INIT_OCCUPANCY_INCONCLUSIVE } wx_init_occupancy_t;

typedef struct {
    size_t struct_size;
    wx_init_stage_t last_stage;
    wx_status_t status, primary_status, cleanup_status;
    wx_init_reason_t reason;
    int primary_errno, release_errno, close_errno;
    char vid[8], pid[8], serial[64], manufacturer[64], product[64], driver[64];
    char node[256], holder_path[256], occupancy_path[256], occupancy_operation[32];
    uint64_t device_rdev;
    wx_init_occupancy_t occupancy;
    bool allow_unknown_occupancy, unknown_accepted;
    bool acquire_succeeded, open_succeeded, exclusive_succeeded, configure_succeeded;
    unsigned baud;
    bool raw_8n1, flow_control_off;
    size_t setter_accepted, query_accepted, fill_accepted, write_calls;
    uint8_t rx[4];
    size_t rx_length;
    bool response_complete, response_valid, orientation_matching;
    bool fill_allowed, fill_attempted, fill_tx_complete;
    uint64_t start_ms, finish_ms, deadline_ms;
    bool seal_called, release_called, release_succeeded, close_called, close_succeeded;
    bool fb_baseline_at_return, visual_clear_confirmed;
    wx_init_state_t state_history[8];
    size_t state_count;
} wx_init_report_t;

typedef struct {
    size_t struct_size;
    wx_transport_t tx;
    wx_status_t (*read)(void *, uint8_t *, size_t, size_t *);
    wx_status_t (*wait_readable)(void *, uint64_t);
    wx_status_t (*probe_quiet)(void *, uint64_t);
    wx_status_t (*check_guards)(void *, wx_init_stage_t, uint64_t, wx_init_report_t *);
    void (*seal)(void *);
    wx_status_t (*release)(void *, wx_init_report_t *);
} wx_init_channel_t;

/* Acquire returns a complete lease on success. On error it MUST clean up its
 * own partial acquisition and leave channel_out empty. Context, clocks and
 * storage are caller-owned; context must outlive the device. Callbacks must
 * not reenter device API. RX and TX share tx.context and the clock timebase.
 * Release is called exactly once after successful acquire, even on validation
 * failure; a successful acquire MUST at least supply a valid release callback.
 * Seal forbids I/O, release frees the lease, never caller-owned TX storage. */
typedef struct {
    size_t struct_size;
    void *context;
    wx_status_t (*now_ms)(void *, uint64_t *);
    wx_status_t (*acquire)(void *, uint64_t, wx_init_channel_t *, wx_init_report_t *);
} wx_init_connector_t;
typedef struct {
    size_t struct_size;
    wx_model_t model;
    uint8_t orientation;
    size_t stride;
    uint64_t timeout_ms; /* Exactly 2000 ms. */
    wx_allocator_t allocator;
    wx_init_connector_t connector;
} wx_init_options_t;
typedef struct {
    size_t struct_size;
    wx_init_state_t state;
    bool managed;
    wx_model_t model;
    uint8_t configured_orientation;
    bool requested_valid;
    uint8_t requested_orientation;
    bool confirmed_valid;
    uint8_t confirmed_orientation;
    size_t confirmed_width, confirmed_height;
    bool geometry_usable;
    wx_fb_sync_t fb_sync;
    wx_init_reason_t last_reason;
} wx_init_info_t;

/* New factories require out != NULL and *out == NULL. Optional report must
 * have struct_size == sizeof(*report), as must every new public structure.
 * FS35 orientation 2 only. init transmits setter, query, validates readback,
 * then black Fill within one 2000 ms budget. Success retains the owned lease.
 * Preserve emits no protocol I/O, leaves geometry unusable until explicit
 * confirm_orientation, and leaves physical framebuffer synchronization unknown.
 * Host TX does not establish visual confirmation. Calls must be serialized.
 * Existing wx_status_t deliberately gains no unsupported status value. */
wx_status_t wx_device_init(const wx_init_options_t *, wx_device_t **, wx_init_report_t *);
wx_status_t wx_device_init_preserve(const wx_init_options_t *, wx_device_t **, wx_init_report_t *);
wx_status_t wx_device_confirm_orientation(wx_device_t *, wx_init_report_t *);
wx_status_t wx_device_get_init_info(const wx_device_t *, wx_init_info_t *);
/* Managed only. Close is idempotent, destroy releases any remaining lease.
 * Report preserves primary error separately from cleanup. A failed release is
 * never retried; callback MUST relinquish ownership even when reporting error.
 * No implicit flush or protocol commands on close/destroy. */
wx_status_t wx_device_init_close(wx_device_t *, wx_init_report_t *);
#endif
