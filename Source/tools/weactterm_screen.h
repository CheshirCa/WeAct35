/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT). Private app module.
 */
#ifndef WEACTTERM_SCREEN_H
#define WEACTTERM_SCREEN_H
#include "weact_extended.h"
#include <vterm.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#define WT_ROWS 20
#define WT_COLS 60
#define WT_CELL_W 8
#define WT_CELL_H 16
#define WT_MAX_ROWS 32
#define WT_MAX_COLS 96
#define WT_MAX_CELL_W 16
#define WT_MAX_CELL_H 32
typedef enum {WT_FONT_SMALL,WT_FONT_NORMAL,WT_FONT_LARGE,WT_FONT_XLARGE,WT_FONT_TINY} wt_font_size;
typedef struct {const char *name;int width,height,pixels,baseline;} wt_font_profile;
const wt_font_profile *wt_font_get(wt_font_size size);
int wt_font_parse(const char *name,wt_font_size *out);
#define WT_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
typedef struct {
    uint32_t code; bool valid; uint8_t alpha[WT_MAX_CELL_W*WT_MAX_CELL_H];
} wt_glyph;
typedef struct {
    const wt_font_profile *font;int rows,cols;
    VTerm *vt;VTermScreen *screen;FT_Library ft;FT_Face face;
    bool dirty,cursor_visible,queue_failed;
    VTermPos cursor;
    bool perf_enabled,render_valid;
    const void *render_pixels;size_t render_stride;
    VTermScreenCell rendered[WT_MAX_ROWS][WT_MAX_COLS];
    bool rendered_cursor[WT_MAX_ROWS][WT_MAX_COLS];
    uint64_t vt_ns,glyph_ns,framebuffer_ns,render_calls,cells_drawn,glyph_misses;
    wt_glyph cache[128];unsigned replacement;
    char outgoing[65536];size_t queued;
} wt_screen;
uint64_t wt_now_ns(void);
uint64_t wt_elapsed_ns(uint64_t);
int wt_screen_open(wt_screen *s);
int wt_screen_open_size(wt_screen *s,wt_font_size size);
void wt_screen_close(wt_screen *s);
int wt_screen_feed(wt_screen *s,const char *p,size_t n);
int wt_screen_render(wt_screen *s,wx_surface_t *surface);
void wt_screen_queue(wt_screen *s,const char *p,size_t n);
#endif
