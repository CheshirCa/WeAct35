/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Internal host-only RGB888 renderer, MIT; see ../LICENSE.
 * Not installed and not exported from libweact.so.1.
 */
#ifndef WEACT_CODES_RENDER_H
#define WEACT_CODES_RENDER_H
#include "weact_codes.h"
#include "weact_graphics.h"

typedef enum {
    WX_RENDER_OK=0, WX_RENDER_INVALID, WX_RENDER_OVERFLOW, WX_RENDER_NO_FIT
} wx_render_status_t;
typedef struct {
    wx_model_t model;
    size_t width,height,stride,length;
    uint8_t *pixels; /* borrowed RGB888; stride=0 selects width*3 */
} wx_code_frame_t;
typedef struct {
    size_t scale;      /* 0=largest fitting integer, otherwise exact */
    size_t min_scale;  /* >=1; default options are {0,1,0} */
    size_t bar_height; /* pixels, 0=maximum; QR requires 0 */
} wx_code_render_options_t;
typedef struct {
    size_t scale;
    size_t x,y,width,height; /* whole symbol including mandatory quiet zones */
    size_t data_x,data_y,data_width,data_height; /* black/white data region */
    size_t min_bar_height,max_bar_height; /* pixels, zero for QR */
} wx_code_layout_t;
typedef struct {
    size_t x,y,width,height; /* FS35 RGB888 pixel rectangle */
} wx_code_region_t;

/* Pure sizing, no allocation. FS35=480x320, FS096=160x80 only.
 * Preserves encoder quiet margins; rejects insufficient metadata. QR cells
 * are square; linear widths are exact integer multiples of source modules.
 * Linear vertical white margin >=1 scaled module each side. Minimum bar
 * height is max(8,ceil(0.15*raw_width)) modules, a geometric policy, NOT a
 * certification of physical millimetres, GS1 compliance or scan reliability.
 * Default maximum bar height uses all remaining screen height. An explicit
 * height must satisfy the same policy. Whole symbol is centered; odd surplus
 * pixels go to the right/bottom. Outputs are unchanged on any error.
 */
wx_render_status_t wx_code_plan(const wx_code_info_t *,wx_model_t,
    const wx_code_render_options_t *,wx_code_layout_t *);
/* Caller owns frame memory, padding is preserved; all actual pixels become
 * white except black modules. No allocation, clipping, resampling or I/O.
 * Source/frame storage must not overlap (checked); layout must not alias
 * either. All validation precedes mutation, failure preserves pixels/layout.
 */
wx_render_status_t wx_code_render(const wx_code_t *,const wx_code_frame_t *,
    const wx_code_render_options_t *,wx_code_layout_t *);
/* FS35-only internal sub-rectangle variant. The rectangle is cleared white
 * only after all encoding, sizing and framebuffer validation succeeds; pixels
 * outside it, framebuffer padding and out are preserved on every failure.
 * The symbol (including mandatory quiet zones) is centered inside it.
 */
wx_render_status_t wx_code_plan_region(const wx_code_info_t *,
    const wx_code_region_t *,const wx_code_render_options_t *,wx_code_layout_t *);
wx_render_status_t wx_code_render_region(const wx_code_t *,const wx_code_frame_t *,
    const wx_code_region_t *,const wx_code_render_options_t *,wx_code_layout_t *);
/* Host-only adapter using the EXISTING graphics API for pixel conversion.
 * Requires model geometry/profile to match; no USB/protocol code here.
 * Copies the full RGB frame, including white background, into owned surface.
 */
wx_status_t wx_code_frame_to_surface(const wx_code_frame_t *,wx_surface_t *);
#endif
