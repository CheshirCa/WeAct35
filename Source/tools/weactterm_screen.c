/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see ../LICENSE (MIT).
 * libvterm alone owns parsing and cells. FreeType rasterizes installed fonts.
 */
#define _POSIX_C_SOURCE 200809L
#include "weactterm_screen.h"
#include <time.h>
#include <string.h>
uint64_t wt_now_ns(void)
{
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0?(uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec:0;
}
uint64_t wt_elapsed_ns(uint64_t start)
{
    uint64_t end=wt_now_ns();return start && end>=start?end-start:0;
}
static int damage(VTermRect r,void *ctx)
{(void)r;((wt_screen *)ctx)->dirty=true;return 1;}
static int moved(VTermRect a,VTermRect b,void *ctx)
{(void)b;return damage(a,ctx);}
static int cursor(VTermPos pos,VTermPos old,int visible,void *ctx)
{(void)old;wt_screen *s=ctx;s->cursor=pos;s->cursor_visible=visible;s->dirty=true;return 1;}
static int property(VTermProp prop,VTermValue *value,void *ctx)
{
    wt_screen *s=ctx;
    if(prop==VTERM_PROP_CURSORVISIBLE)s->cursor_visible=value->boolean;
    s->dirty=true;return 1;
}
/* No scrollback UI in FAST: current-screen scroll remains libvterm-owned. */
static int push(int cols,const VTermScreenCell *cells,void *ctx)
{(void)cols;(void)cells;(void)ctx;return 1;}
static int pop(int cols,VTermScreenCell *cells,void *ctx)
{(void)cols;(void)cells;(void)ctx;return 0;}
void wt_screen_queue(wt_screen *s,const char *p,size_t n)
{
    if(n>sizeof(s->outgoing)-s->queued){s->queue_failed=true;return;}
    memcpy(s->outgoing+s->queued,p,n);s->queued+=n;
}
static void output(const char *p,size_t n,void *ctx) {wt_screen_queue(ctx,p,n);}
static const VTermScreenCallbacks callbacks={.damage=damage,.moverect=moved,
    .movecursor=cursor,.settermprop=property,.sb_pushline=push,.sb_popline=pop};
static const wt_font_profile profiles[]={
    {"small",6,12,9,9},{"normal",8,16,12,12},
    {"large",12,24,18,18},{"xlarge",16,32,24,24},
    {"tiny",5,10,8,8}
};
const wt_font_profile *wt_font_get(wt_font_size size)
{return (unsigned)size<sizeof(profiles)/sizeof(profiles[0])?&profiles[size]:NULL;}
int wt_font_parse(const char *name,wt_font_size *out)
{
    for(unsigned n=0;n<sizeof(profiles)/sizeof(profiles[0]);++n)
        if(!strcmp(name,profiles[n].name)){*out=(wt_font_size)n;return 0;}
    return -1;
}
int wt_screen_open(wt_screen *s) {return wt_screen_open_size(s,WT_FONT_NORMAL);}
int wt_screen_open_size(wt_screen *s,wt_font_size size)
{
    memset(s,0,sizeof(*s));s->cursor_visible=true;s->font=wt_font_get(size);
    if(!s->font)return -1;
    s->rows=320/s->font->height;s->cols=480/s->font->width;
    if(FT_Init_FreeType(&s->ft) || FT_New_Face(s->ft,WT_FONT,0,&s->face) ||
       FT_Set_Pixel_Sizes(s->face,0,(unsigned)s->font->pixels) || !FT_IS_FIXED_WIDTH(s->face))goto failed;
    if(!FT_Get_Char_Index(s->face,0x410) || !FT_Get_Char_Index(s->face,0x451))goto failed;
    s->vt=vterm_new(s->rows,s->cols);if(!s->vt)goto failed;
    vterm_set_utf8(s->vt,1);s->screen=vterm_obtain_screen(s->vt);if(!s->screen)goto failed;
    vterm_output_set_callback(s->vt,output,s);
    vterm_screen_set_callbacks(s->screen,&callbacks,s);
    vterm_screen_enable_altscreen(s->screen,1);
    VTermColor fg,bg;vterm_color_rgb(&fg,224,224,224);vterm_color_rgb(&bg,0,0,0);
    vterm_state_set_default_colors(vterm_obtain_state(s->vt),&fg,&bg);
    vterm_screen_reset(s->screen,1);vterm_screen_flush_damage(s->screen);
    s->dirty=true;return 0;
failed:wt_screen_close(s);return -1;
}
void wt_screen_close(wt_screen *s)
{
    if(s->vt)vterm_free(s->vt);
    if(s->face)FT_Done_Face(s->face);
    if(s->ft)FT_Done_FreeType(s->ft);
    memset(s,0,sizeof(*s));
}
int wt_screen_feed(wt_screen *s,const char *p,size_t n)
{
    uint64_t start=s->perf_enabled?wt_now_ns():0;
    if(vterm_input_write(s->vt,p,n)!=n)return -1;
    vterm_screen_flush_damage(s->screen);
    if(s->perf_enabled)s->vt_ns+=wt_elapsed_ns(start);
    return s->queue_failed?-1:0;
}
static const uint8_t *glyph(wt_screen *s,uint32_t code)
{
    for(size_t n=0;n<128;++n)if(s->cache[n].valid && s->cache[n].code==code)return s->cache[n].alpha;
    uint64_t start=s->perf_enabled?wt_now_ns():0;
    ++s->glyph_misses;
    wt_glyph *g=&s->cache[s->replacement++%128];memset(g,0,sizeof(*g));
    g->valid=true;g->code=code;
    FT_UInt index=FT_Get_Char_Index(s->face,code);
    if(!index)index=FT_Get_Char_Index(s->face,0xfffd);
    if(FT_Load_Glyph(s->face,index,FT_LOAD_DEFAULT) || FT_Render_Glyph(s->face->glyph,FT_RENDER_MODE_NORMAL))goto done;
    FT_GlyphSlot slot=s->face->glyph;FT_Bitmap *b=&slot->bitmap;
    if(b->pixel_mode!=FT_PIXEL_MODE_GRAY)goto done;
    int pitch=b->pitch,abs_pitch=pitch<0?-pitch:pitch;
    int cw=s->font->width,ch=s->font->height;
    int left=slot->bitmap_left,top=s->font->baseline-slot->bitmap_top;
    unsigned width=b->width,height=b->rows;
    if(s->font!=&profiles[WT_FONT_NORMAL]) {
        /* Fit the entire ink box, including overhanging line-drawing glyphs.
         * Area maxima retain thin strokes when a glyph needs compression. */
        if(left<0)left=0;
        if(top<0)top=0;
        if(left>=cw)left=0;
        if(top>=ch)top=0;
        if(width>(unsigned)(cw-left))width=(unsigned)(cw-left);
        if(height>(unsigned)(ch-top))height=(unsigned)(ch-top);
    }
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        int dx=left+(int)x,dy=top+(int)y;
        if(dx<0 || dx>=cw || dy<0 || dy>=ch)continue;
        unsigned y0=y*b->rows/height,y1=((y+1)*b->rows+height-1)/height;
        unsigned x0=x*b->width/width,x1=((x+1)*b->width+width-1)/width;
        uint8_t alpha=0;
        for(unsigned iy=y0;iy<y1;++iy)for(unsigned ix=x0;ix<x1;++ix) {
            unsigned sy=pitch<0?b->rows-1-iy:iy;
            uint8_t value=b->buffer[(size_t)sy*(size_t)abs_pitch+ix];
            if(value>alpha)alpha=value;
        }
        g->alpha[dy*cw+dx]=alpha;
    }
done:
    if(s->perf_enabled)s->glyph_ns+=wt_elapsed_ns(start);
    return g->alpha;
}
static bool same_cell(const VTermScreenCell *a,const VTermScreenCell *b)
{
    for(int n=0;n<VTERM_MAX_CHARS_PER_CELL;++n) {
        if(a->chars[n]!=b->chars[n])return false;
        if(!a->chars[n])break; /* libvterm does not promise unused slots. */
    }
    return a->width==b->width &&
        a->attrs.bold==b->attrs.bold && a->attrs.underline==b->attrs.underline &&
        a->attrs.reverse==b->attrs.reverse && a->attrs.conceal==b->attrs.conceal &&
        vterm_color_is_equal(&a->fg,&b->fg) && vterm_color_is_equal(&a->bg,&b->bg);
}
int wt_screen_render(wt_screen *s,wx_surface_t *surface)
{
    wx_surface_info_t info;
    if(wx_surface_info(surface,&info)!=WX_OK || info.width!=480 || info.height!=320 || info.format!=WX_RGB565_LE)return -1;
    uint8_t *pixels=wx_surface_pixels(surface,NULL);
    bool valid=s->render_valid && pixels==s->render_pixels && info.stride==s->render_stride;
    uint64_t start=s->perf_enabled?wt_now_ns():0,glyph_start=s->glyph_ns;
    ++s->render_calls;
    int cw=s->font->width,ch=s->font->height;
    if(!valid)for(unsigned y=0;y<320;++y)memset(pixels+y*info.stride,0,960);
    for(int row=0;row<s->rows;++row)for(int col=0;col<s->cols;++col) {
        VTermScreenCell cell={0};
        if(!vterm_screen_get_cell(s->screen,(VTermPos){row,col},&cell))return -1;
        bool cursor_here=s->cursor_visible && row==s->cursor.row && col==s->cursor.col;
        if(valid && cursor_here==s->rendered_cursor[row][col] &&
           same_cell(&cell,&s->rendered[row][col]))continue;
        s->rendered[row][col]=cell;s->rendered_cursor[row][col]=cursor_here;
        ++s->cells_drawn;
        VTermColor fg=cell.fg,bg=cell.bg;
        vterm_screen_convert_color_to_rgb(s->screen,&fg);vterm_screen_convert_color_to_rgb(s->screen,&bg);
        if(cell.attrs.reverse){VTermColor swap=fg;fg=bg;bg=swap;}
        uint8_t mask[WT_MAX_CELL_W*WT_MAX_CELL_H]={0};
        for(int n=0;n<VTERM_MAX_CHARS_PER_CELL && cell.chars[n];++n) {
            if(cell.chars[n]==UINT32_MAX)continue;
            const uint8_t *a=glyph(s,cell.chars[n]);
            for(int k=0;k<cw*ch;++k)if(a[k]>mask[k])mask[k]=a[k];
        }
        for(int y=0;y<ch;++y)for(int x=0;x<cw;++x) {
            unsigned a=cell.attrs.conceal?0:mask[y*cw+x];
            if(cell.attrs.bold && x && !cell.attrs.conceal && mask[y*cw+x-1]>a)a=mask[y*cw+x-1];
            if(cell.attrs.underline && y==ch-2 && !cell.attrs.conceal)a=255;
            if(cursor_here && y>=ch-2)a=255;
            uint8_t r=(uint8_t)((fg.rgb.red*a+bg.rgb.red*(255-a))/255);
            uint8_t g=(uint8_t)((fg.rgb.green*a+bg.rgb.green*(255-a))/255);
            uint8_t b=(uint8_t)((fg.rgb.blue*a+bg.rgb.blue*(255-a))/255);
            uint16_t color=wx_pack_rgb(WX_RGB565_LE,r,g,b);
            size_t offset=(size_t)(row*ch+y)*info.stride+(size_t)(col*cw+x)*2;
            pixels[offset]=(uint8_t)color;pixels[offset+1]=(uint8_t)(color>>8);
        }
    }
    s->render_valid=true;s->render_pixels=pixels;s->render_stride=info.stride;
    if(s->perf_enabled) {
        uint64_t total=wt_elapsed_ns(start),raster=s->glyph_ns-glyph_start;
        if(total>=raster)s->framebuffer_ns+=total-raster;
    }
    s->dirty=false;return 0;
}
