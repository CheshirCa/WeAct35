/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Standalone experimental encoder, MIT; see LICENSE. Not libweact.so.1 ABI.
 */
#ifndef WEACT_CODES_H
#define WEACT_CODES_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WX_CODE_OK = 0, WX_CODE_INVALID, WX_CODE_EMPTY, WX_CODE_TOO_LONG,
    WX_CODE_CHARACTER, WX_CODE_CHECKSUM, WX_CODE_NOMEM, WX_CODE_BACKEND
} wx_code_status_t;
typedef enum {
    WX_CODE_QR = 1, WX_CODE_128, WX_CODE_EAN8, WX_CODE_EAN13, WX_CODE_39
} wx_code_format_t;
typedef enum { WX_QR_L = 0, WX_QR_M, WX_QR_Q, WX_QR_H } wx_qr_ecc_t;
typedef struct wx_code wx_code_t;
typedef struct {
    void *context;
    void *(*allocate)(void *, size_t);
    void (*release)(void *, void *);
} wx_code_allocator_t;
typedef struct {
    wx_qr_ecc_t ecc;
    unsigned min_version, max_version; /* 1..40, inclusive */
} wx_qr_options_t;
typedef struct {
    wx_code_format_t format;
    size_t width, height, stride; /* raw modules, one byte per module: 0/1 */
    size_t quiet_left, quiet_right, quiet_top, quiet_bottom;
    unsigned qr_version;         /* 0 for linear symbols */
    int check_digit;             /* EAN 0..9, Code128 0..102, otherwise -1 */
} wx_code_info_t;

/* Explicit byte length, input borrowed only during call. QR: strictly valid
 * UTF-8, ECI 26, byte mode; embedded NUL is encoded, no normalization/BOM.
 * Code128: ASCII 0..127 (including NUL), optimal A/B/C + SHIFT, no FNC/GS1.
 * EAN: 7/8 or 12/13 digits (compute/validate checksum). Code39: basic alphabet
 * 0-9 A-Z space - . $ / + %, no '*' in input, ratio 2:1, no Mod43.
 * Empty input is rejected. Limits: QR 2953 bytes; linear input 1024 bytes.
 * options must be NULL for linear formats; QR NULL defaults to M, 1..40.
 * Allocator callbacks must both exist or both be NULL; NULL = malloc/free.
 * Returned storage must have malloc-compatible alignment. Callbacks/context
 * must outlive the code. They cover owned/scratch allocations, not backend
 * internals; all sizes passed to allocate are nonzero.
 * out must point to NULL; failure preserves it. Destroy accepts NULL.
 * Serialize QR calls (backend global caches); backend errno is translated.
 */
wx_code_status_t wx_code_encode(wx_code_format_t format, const uint8_t *data,
    size_t length, const wx_qr_options_t *options,
    const wx_code_allocator_t *allocator, wx_code_t **out);
void wx_code_destroy(wx_code_t *code);
wx_code_status_t wx_code_info(const wx_code_t *code, wx_code_info_t *out);
/* Borrowed immutable row-major storage, length=height*stride. */
const uint8_t *wx_code_modules(const wx_code_t *code, size_t *length);

/* Quiet zones are mandatory metadata, NOT included in raw storage. Future
 * renderers must reserve them, use integer modules, no smoothing or cropping.
 * Code39 additionally needs at least 2.54 mm quiet space each side: a physical
 * constraint the module encoder cannot resolve. No scale/bar-height promises.
 */
#ifdef __cplusplus
}
#endif
#endif
