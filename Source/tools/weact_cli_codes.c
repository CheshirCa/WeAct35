/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * GPL-3.0-only CLI glue for existing MIT encoders/renderer. No transport code.
 */
#include "weact_cli.h"
#include "codes/render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int cli_code_encode(const cli_options *o,wx_code_t **out)
{
    if(!o || !o->code_format || !o->code_data || !out)return -1;
    wx_code_status_t status=wx_code_encode(o->code_format,(const uint8_t *)o->code_data,
        strlen(o->code_data),NULL,NULL,out);
    if(status==WX_CODE_OK)return 0;
    const char *message;
    switch(status) {
    case WX_CODE_EMPTY:message="empty code data";break;
    case WX_CODE_TOO_LONG:message="code data exceeds encoder capacity";break;
    case WX_CODE_CHARACTER:
        message=o->code_format==WX_CODE_QR?"QR data is not valid UTF-8":
            o->code_format==WX_CODE_128?"Code128 accepts ASCII bytes only (0..127)":
            o->code_format==WX_CODE_39?"Code39 accepts 0-9 A-Z space - . $ / + %, without '*'":
            "EAN accepts decimal digits only";break;
    case WX_CODE_CHECKSUM:message="EAN checksum mismatch";break;
    case WX_CODE_NOMEM:message="not enough memory to encode code";break;
    case WX_CODE_BACKEND:message="QR backend failed or is unavailable";break;
    case WX_CODE_INVALID:
        message=o->code_format==WX_CODE_EAN8?"EAN-8 requires 7 data digits or 8 digits including checksum":
            o->code_format==WX_CODE_EAN13?"EAN-13 requires 12 data digits or 13 digits including checksum":
            "invalid code arguments";break;
    default:message="invalid code arguments";break;
    }
    fprintf(stderr,"Code input error: %s\n",message);return -1;
}
int cli_code_canvas(const wx_code_t *code,int model,cli_canvas *out)
{
    if(!code || !out || out->rgb || out->mask || (model!=WX_FS35 && model!=WX_FS096))return -1;
    wx_code_info_t info;wx_code_layout_t layout;
    if(wx_code_info(code,&info)!=WX_CODE_OK)return -1;
    wx_render_status_t st=wx_code_plan(&info,(wx_model_t)model,NULL,&layout);
    if(st!=WX_RENDER_OK) {
        fprintf(stderr,"Code does not fit %s (%s; integer modules and quiet zones retained), render_status=%d\n",
            model==WX_FS35?"FS35":"FS096",model==WX_FS35?"480x320":"160x80",st);return -1;
    }
    int w=model==WX_FS35?480:160,h=model==WX_FS35?320:80;
    unsigned char *rgb=malloc((size_t)w*(size_t)h*3);
    if(!rgb){fputs("Not enough memory for code framebuffer\n",stderr);return -1;}
    wx_code_frame_t frame={(wx_model_t)model,(size_t)w,(size_t)h,0,(size_t)w*(size_t)h*3,rgb};
    st=wx_code_render(code,&frame,NULL,&layout);
    if(st!=WX_RENDER_OK){free(rgb);fprintf(stderr,"Code rendering failed: status=%d\n",st);return -1;}
    *out=(cli_canvas){.rgb=rgb,.width=w,.height=h};
    return 0;
}
wx_status_t cli_code_send(wx_device_t *device,const cli_canvas *c)
{
    wx_model_t model;wx_surface_info_t info;
    if(!device || !c || !c->rgb || wx_device_profile(device,&model,&info)!=WX_OK ||
        c->width!=(int)info.width || c->height!=(int)info.height)return WX_INVALID;
    wx_code_frame_t frame={model,info.width,info.height,0,info.width*info.height*3,c->rgb};
    wx_status_t status=wx_code_frame_to_surface(&frame,wx_device_surface(device));
    return status==WX_OK?wx_device_bitmap(device):status; /* Single raw bitmap, fail-stop. */
}
/* Validate again at presentation: callers cannot bypass model/bounds checks. */
static int code_region(const cli_options *o,int model,wx_code_region_t *r)
{
    if(!o || !o->code_region || model!=WX_FS35 || o->x<0 || o->y<0 ||
       o->x>=480 || o->y>=320 || o->width<=0 || o->height<=0 ||
       o->width>480-o->x || o->height>320-o->y)return -1;
    *r=(wx_code_region_t){(size_t)o->x,(size_t)o->y,(size_t)o->width,(size_t)o->height};
    return 0;
}
int cli_code_prepare(const cli_options *o,const wx_code_t *code,int model,cli_canvas *out)
{
    if(!o)return -1;
    if(!o->code_region)return cli_code_canvas(code,model,out);
    wx_code_region_t r;wx_code_info_t info;wx_code_layout_t layout;
    if(code_region(o,model,&r)) {
        fputs("Code region requires FS35 landscape 480x320 and an in-bounds nonempty rectangle\n",stderr);
        return -1;
    }
    if(!code || !out || out->rgb || out->mask || wx_code_info(code,&info)!=WX_CODE_OK)return -1;
    wx_render_status_t st=wx_code_plan_region(&info,&r,NULL,&layout);
    if(st!=WX_RENDER_OK) {
        fprintf(stderr,"Code does not fit region (integer modules and quiet zones retained), render_status=%d\n",st);
        return -1;
    }
    /* Host scratch only; unknown LCD pixels are never read or reconstructed. */
    unsigned char *rgb=calloc(480u*320u,3);
    if(!rgb){fputs("Not enough memory for code region framebuffer\n",stderr);return -1;}
    wx_code_frame_t frame={WX_FS35,480,320,0,480u*320u*3,rgb};
    st=wx_code_render_region(code,&frame,&r,NULL,&layout);
    if(st!=WX_RENDER_OK){free(rgb);fprintf(stderr,"Code region rendering failed: status=%d\n",st);return -1;}
    *out=(cli_canvas){.rgb=rgb,.width=480,.height=320};return 0;
}
wx_status_t cli_code_region_send(wx_device_t *device,const cli_options *o,const cli_canvas *c)
{
    wx_model_t model;wx_surface_info_t info;wx_code_region_t r;
    if(!device || !c || !c->rgb || c->width!=480 || c->height!=320 ||
       wx_device_profile(device,&model,&info)!=WX_OK || info.width!=480 || info.height!=320 ||
       code_region(o,model,&r))return WX_INVALID;
    /* Convert one RGB row at a time, preserving host surface outside too.
     * This avoids allocation after open and satisfies bitmap_rgb's source
     * length contract even at the bottom/right edges. No I/O until complete. */
    for(size_t y=0;y<r.height;y++) {
        wx_status_t status=wx_surface_bitmap_rgb(wx_device_surface(device),r.x,r.y+y,r.width,1,
            c->rgb+((r.y+y)*480+r.x)*3,r.width*3,0);
        if(status!=WX_OK)return status;
    }
    return wx_device_bitmap_region(device,r.x,r.y,r.width,r.height);
}
