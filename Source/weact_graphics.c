/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest */
#include "weact_internal.h"
#include "weact_graphics.h"
wx_status_t wx_device_profile(const wx_device_t *d, wx_model_t *m, wx_surface_info_t *i)
{
    if (!d || !m || !i) return WX_INVALID;
    *m=d->model; return wx_surface_info(d->surface,i);
}
static wx_status_t bounds(wx_surface_t *s,size_t x,size_t y,size_t w,size_t h)
{
    if (!s) return WX_INVALID;
    if (x>s->info.width || y>s->info.height || w>s->info.width-x || h>s->info.height-y)
        return WX_BOUNDS;
    return WX_OK;
}
wx_status_t wx_surface_fill_rgb(wx_surface_t *s,size_t x,size_t y,size_t w,size_t h,
                              uint8_t r,uint8_t g,uint8_t b)
{
    wx_status_t st=bounds(s,x,y,w,h); if(st!=WX_OK)return st;
    if(x>INT32_MAX || y>INT32_MAX || w>INT32_MAX-x || h>INT32_MAX-y)return WX_OVERFLOW;
    for(size_t j=0;j<h;j++)for(size_t i=0;i<w;i++)
        wx_surface_set_rgb(s,(int)(x+i),(int)(y+j),r,g,b);
    return WX_OK;
}
wx_status_t wx_surface_bitmap_rgb(wx_surface_t *s,size_t x,size_t y,size_t w,size_t h,
                                const uint8_t *p,size_t length,size_t stride)
{
    wx_status_t st=bounds(s,x,y,w,h); if(st!=WX_OK)return st;
    if(!w || !h)return WX_OK;
    if(w>SIZE_MAX/3)return WX_OVERFLOW;
    if(!stride)stride=w*3;
    if(stride<w*3)return WX_INVALID;
    if(h>SIZE_MAX/stride)return WX_OVERFLOW;
    if(!p || length<h*stride)return WX_INVALID;
    if(x>INT32_MAX || y>INT32_MAX || w>INT32_MAX-x || h>INT32_MAX-y)return WX_OVERFLOW;
    for(size_t j=0;j<h;j++)for(size_t i=0;i<w;i++) {
        const uint8_t *c=p+j*stride+i*3;
        wx_surface_set_rgb(s,(int)(x+i),(int)(y+j),c[0],c[1],c[2]);
    }
    return WX_OK;
}
