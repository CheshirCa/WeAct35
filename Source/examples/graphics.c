/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest */
#include <weact_graphics.h>
#include <stdio.h>
/* Offline canvas example: never opens a serial device. */
int main(void)
{
    wx_surface_t *s=NULL; wx_surface_info_t i;
    if(wx_surface_create(160,80,0,WX_LEGACY_BRG565_BE,NULL,&s)!=WX_OK)return 1;
    int result=1;
    if(wx_surface_info(s,&i)!=WX_OK)goto done;
    printf("%zux%zu\n",i.width,i.height);
    if(wx_surface_fill_rgb(s,0,0,i.width,i.height,0,0,0)!=WX_OK)goto done;
    if(wx_surface_fill_rgb(s,0,0,i.width/2,i.height/2,255,0,0)!=WX_OK)goto done;
    if(wx_surface_set_rgb(s,(int)i.width-1,(int)i.height-1,0,255,0)!=WX_OK)goto done;
    result=0;
done:
    wx_surface_destroy(s); return result;
}
