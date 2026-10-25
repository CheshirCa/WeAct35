/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * MIT, internal RGB888 rasterization; encoding stays in weact_codes.c.
 */
#include "codes/render.h"
#include <stdint.h>
#include <string.h>

static int add(size_t a,size_t b,size_t *out)
{ if (b>SIZE_MAX-a) return 0; *out=a+b; return 1; }
static int mul(size_t a,size_t b,size_t *out)
{ if (a && b>SIZE_MAX/a) return 0; *out=a*b; return 1; }
static int geometry(wx_model_t model,size_t *w,size_t *h)
{
    if (model==WX_FS35) { *w=480; *h=320; return 1; }
    if (model==WX_FS096) { *w=160; *h=80; return 1; }
    return 0;
}
static size_t minimum(size_t a,size_t b) { return a<b?a:b; }
static wx_render_status_t plan_area(const wx_code_info_t *i,wx_model_t model,
    size_t aw,size_t ah,size_t ox,size_t oy,const wx_code_render_options_t *options,
    wx_code_layout_t *out)
{
    if (!i || !out) return WX_RENDER_INVALID;
    size_t fw,fh,capacity;
    if (!geometry(model,&fw,&fh) || !aw || !ah || aw>fw || ah>fh || ox>fw-aw || oy>fh-ah ||
        !i->width || !i->height || i->stride<i->width ||
        i->format<WX_CODE_QR || i->format>WX_CODE_39) return WX_RENDER_INVALID;
    if (!mul(i->stride,i->height,&capacity)) return WX_RENDER_OVERFLOW;
    wx_code_render_options_t o=options?*options:(wx_code_render_options_t){0,1,0};
    if (!o.min_scale) return WX_RENDER_INVALID;
    int qr=i->format==WX_CODE_QR;
    if (qr) {
        if (o.bar_height || i->qr_version<1 || i->qr_version>40 ||
            i->width!=17+4*i->qr_version || i->height!=i->width ||
            i->quiet_left<4 || i->quiet_right<4 || i->quiet_top<4 || i->quiet_bottom<4)
            return WX_RENDER_INVALID;
    } else {
        size_t ql=i->format==WX_CODE_EAN13?11:i->format==WX_CODE_EAN8?7:10;
        size_t qr_min=i->format==WX_CODE_EAN13 || i->format==WX_CODE_EAN8?7:10;
        if (i->height!=1 || i->quiet_left<ql || i->quiet_right<qr_min) return WX_RENDER_INVALID;
    }
    size_t top=i->quiet_top,bottom=i->quiet_bottom,tw,th,t;
    if (!qr) { if (!top) top=1; if (!bottom) bottom=1; }
    if (!add(i->width,i->quiet_left,&t) || !add(t,i->quiet_right,&tw)) return WX_RENDER_OVERFLOW;
    /* ceil(3*width/20) without overflowing 3*width. */
    size_t min_bar_units=(i->width/20)*3+((i->width%20)*3+19)/20;
    if (min_bar_units<8) min_bar_units=8;
    if (!add(qr?i->height:min_bar_units,top,&t) || !add(t,bottom,&th)) return WX_RENDER_OVERFLOW;
    size_t max_scale=minimum(aw/tw,ah/th);
    if (!qr && o.bar_height) {
        size_t margins,required;
        if (!add(top,bottom,&margins) || !add(o.bar_height,margins,&required)) return WX_RENDER_OVERFLOW;
        if (o.bar_height<min_bar_units) return WX_RENDER_INVALID;
        if (required>ah) return WX_RENDER_NO_FIT;
        max_scale=minimum(max_scale,(ah-o.bar_height)/margins);
        max_scale=minimum(max_scale,o.bar_height/min_bar_units);
    }
    size_t scale=o.scale?o.scale:max_scale;
    wx_code_layout_t p={.scale=scale};
    size_t top_px,bottom_px,left_px;
    if (!mul(tw,scale,&p.width) || !mul(th,scale,&p.height) ||
        !mul(top,scale,&top_px) || !mul(bottom,scale,&bottom_px) ||
        !mul(i->quiet_left,scale,&left_px) || !mul(i->width,scale,&p.data_width))
        return WX_RENDER_OVERFLOW;
    if (!scale || scale<o.min_scale || scale>max_scale) return WX_RENDER_NO_FIT;
    if (qr) {
        if (!mul(i->height,scale,&p.data_height)) return WX_RENDER_OVERFLOW;
    } else {
        if (!mul(min_bar_units,scale,&p.min_bar_height)) return WX_RENDER_OVERFLOW;
        p.max_bar_height=ah-top_px-bottom_px;
        p.data_height=o.bar_height?o.bar_height:p.max_bar_height;
        if (!add(p.data_height,top_px,&t) || !add(t,bottom_px,&p.height)) return WX_RENDER_OVERFLOW;
        if (p.data_height<p.min_bar_height) return WX_RENDER_INVALID;
        if (p.data_height>p.max_bar_height) return WX_RENDER_NO_FIT;
    }
    if (p.width>aw || p.height>ah || !add(ox,(aw-p.width)/2,&p.x) ||
        !add(oy,(ah-p.height)/2,&p.y) || !add(p.x,left_px,&p.data_x) ||
        !add(p.y,top_px,&p.data_y)) return WX_RENDER_OVERFLOW;
    *out=p;
    return WX_RENDER_OK;
}
wx_render_status_t wx_code_plan(const wx_code_info_t *i,wx_model_t model,
    const wx_code_render_options_t *options,wx_code_layout_t *out)
{
    size_t w,h;
    if (!geometry(model,&w,&h)) return WX_RENDER_INVALID;
    return plan_area(i,model,w,h,0,0,options,out);
}
static wx_render_status_t region_valid(const wx_code_region_t *r)
{
    if (!r || !r->width || !r->height || r->x>=480 || r->y>=320 ||
        r->width>480-r->x || r->height>320-r->y) return WX_RENDER_INVALID;
    return WX_RENDER_OK;
}
wx_render_status_t wx_code_plan_region(const wx_code_info_t *i,
    const wx_code_region_t *r,const wx_code_render_options_t *options,wx_code_layout_t *out)
{
    wx_render_status_t status=region_valid(r);
    if (status!=WX_RENDER_OK) return status;
    return plan_area(i,WX_FS35,r->width,r->height,r->x,r->y,options,out);
}
static wx_render_status_t frame_valid(const wx_code_frame_t *f,size_t *stride,size_t *capacity)
{
    if (!f || !f->pixels) return WX_RENDER_INVALID;
    size_t row,w,h;
    if (!mul(f->width,3,&row)) return WX_RENDER_OVERFLOW;
    *stride=f->stride?f->stride:row;
    if (!mul(f->height,*stride,capacity)) return WX_RENDER_OVERFLOW;
    if (!geometry(f->model,&w,&h) || f->width!=w || f->height!=h ||
        *stride<row || f->length<*capacity) return WX_RENDER_INVALID;
    if (*capacity>UINTPTR_MAX-(uintptr_t)f->pixels) return WX_RENDER_OVERFLOW;
    return WX_RENDER_OK;
}
static wx_render_status_t render_planned(const wx_code_t *code,const wx_code_frame_t *f,
    size_t stride,size_t capacity,const wx_code_info_t *info,const wx_code_layout_t *p,
    size_t clear_x,size_t clear_y,size_t clear_w,size_t clear_h,wx_code_layout_t *out)
{
    size_t length;
    const uint8_t *modules=wx_code_modules(code,&length);
    size_t needed;
    if (!modules || !mul(info->stride,info->height,&needed) || length<needed) return WX_RENDER_INVALID;
    uintptr_t a=(uintptr_t)modules,b=(uintptr_t)f->pixels;
    if (length>UINTPTR_MAX-a) return WX_RENDER_OVERFLOW;
    if ((a<=b && b-a<length) || (b<a && a-b<capacity)) return WX_RENDER_INVALID;
    for (size_t y=0;y<info->height;y++) for (size_t x=0;x<info->width;x++)
        if (modules[y*info->stride+x]>1) return WX_RENDER_INVALID;
    for (size_t y=0;y<clear_h;y++) memset(f->pixels+(clear_y+y)*stride+clear_x*3,255,clear_w*3);
    for (size_t y=0;y<info->height;y++) for (size_t x=0;x<info->width;x++) {
        if (!modules[y*info->stride+x]) continue;
        size_t px=p->data_x+x*p->scale;
        size_t py=p->data_y+(info->format==WX_CODE_QR?y*p->scale:0);
        size_t height=info->format==WX_CODE_QR?p->scale:p->data_height;
        for (size_t row=0;row<height;row++) memset(f->pixels+(py+row)*stride+px*3,0,p->scale*3);
    }
    *out=*p;
    return WX_RENDER_OK;
}
wx_render_status_t wx_code_render(const wx_code_t *code,const wx_code_frame_t *f,
    const wx_code_render_options_t *options,wx_code_layout_t *out)
{
    if (!code || !out) return WX_RENDER_INVALID;
    size_t stride,capacity;
    wx_render_status_t status=frame_valid(f,&stride,&capacity);
    if (status!=WX_RENDER_OK) return status;
    wx_code_info_t info;
    if (wx_code_info(code,&info)!=WX_CODE_OK) return WX_RENDER_INVALID;
    wx_code_layout_t p;
    status=wx_code_plan(&info,f->model,options,&p);
    if (status!=WX_RENDER_OK) return status;
    return render_planned(code,f,stride,capacity,&info,&p,0,0,f->width,f->height,out);
}
wx_render_status_t wx_code_render_region(const wx_code_t *code,const wx_code_frame_t *f,
    const wx_code_region_t *r,const wx_code_render_options_t *options,wx_code_layout_t *out)
{
    if (!code || !out) return WX_RENDER_INVALID;
    wx_render_status_t status=region_valid(r);
    if (status!=WX_RENDER_OK) return status;
    size_t stride,capacity;
    status=frame_valid(f,&stride,&capacity);
    if (status!=WX_RENDER_OK) return status;
    if (f->model!=WX_FS35) return WX_RENDER_INVALID;
    wx_code_info_t info;
    if (wx_code_info(code,&info)!=WX_CODE_OK) return WX_RENDER_INVALID;
    wx_code_layout_t p;
    status=wx_code_plan_region(&info,r,options,&p);
    if (status!=WX_RENDER_OK) return status;
    return render_planned(code,f,stride,capacity,&info,&p,r->x,r->y,r->width,r->height,out);
}
wx_status_t wx_code_frame_to_surface(const wx_code_frame_t *f,wx_surface_t *s)
{
    size_t stride,capacity;
    wx_render_status_t st=frame_valid(f,&stride,&capacity);
    if (st!=WX_RENDER_OK) return st==WX_RENDER_OVERFLOW?WX_OVERFLOW:WX_INVALID;
    wx_surface_info_t info;
    if (wx_surface_info(s,&info)!=WX_OK || info.width!=f->width || info.height!=f->height ||
        info.format!=(f->model==WX_FS35?WX_RGB565_LE:WX_LEGACY_BRG565_BE)) return WX_INVALID;
    size_t target_length;
    const uint8_t *target=wx_surface_pixels(s,&target_length);
    uintptr_t a=(uintptr_t)f->pixels,b=(uintptr_t)target;
    if (!target || target_length>UINTPTR_MAX-b) return WX_INVALID;
    if ((a<=b && b-a<capacity) || (b<a && a-b<target_length)) return WX_INVALID;
    return wx_surface_bitmap_rgb(s,0,0,f->width,f->height,f->pixels,capacity,stride);
}
