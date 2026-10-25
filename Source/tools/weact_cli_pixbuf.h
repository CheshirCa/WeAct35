/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * GPL-3.0-only. Minimal declarations of the stable GdkPixbuf 2.0 C ABI.
 * No implementation copied. Opaque objects, no access to library layouts.
 * System runtime is present; these functions need no GLib development headers.
 * References: docs.gtk.org/gdk-pixbuf/class.PixbufLoader.html
 */
#ifndef WEACT_CLI_PIXBUF_H
#define WEACT_CLI_PIXBUF_H
#include <stddef.h>
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkPixbufLoader GdkPixbufLoader;
typedef struct _GError GError;
extern GdkPixbufLoader *gdk_pixbuf_loader_new(void);
extern int gdk_pixbuf_loader_write(GdkPixbufLoader *,const unsigned char *,size_t,GError **);
extern int gdk_pixbuf_loader_close(GdkPixbufLoader *,GError **);
extern void gdk_pixbuf_loader_set_size(GdkPixbufLoader *,int,int);
extern GdkPixbuf *gdk_pixbuf_loader_get_pixbuf(GdkPixbufLoader *);
extern int gdk_pixbuf_get_width(const GdkPixbuf *);
extern int gdk_pixbuf_get_height(const GdkPixbuf *);
extern int gdk_pixbuf_get_rowstride(const GdkPixbuf *);
extern int gdk_pixbuf_get_n_channels(const GdkPixbuf *);
extern int gdk_pixbuf_get_bits_per_sample(const GdkPixbuf *);
extern unsigned char *gdk_pixbuf_get_pixels(const GdkPixbuf *);
extern void g_object_unref(void *);
extern unsigned long g_signal_connect_data(void *,const char *,void (*)(void),void *,void (*)(void *,void *),int);
#endif
