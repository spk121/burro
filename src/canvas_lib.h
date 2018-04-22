#ifndef CANVAS_LIB_H
#define CANVAS_LIB_H

/* This is just a game.  It is not DO-178C. Let's put all of our
 * parameter checking paranoia in here, so it doesn't clutter up the
 * rest of the code. */
#if HAVE_CONFIG_H
#include "../config.h"
#endif
#include <cairo.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#ifdef HAVE_LIBPULSE
#include <pulse/pulseaudio.h>
#endif
#ifdef USE_GLIB_MAINLOOP
# include <pulse/glib-mainloop.h>
#endif
#include "visibility.h"


DLL_LOCAL cairo_surface_t * xcairo_image_surface_create (cairo_format_t format,
							 int width,
							 int height);
DLL_LOCAL guint32 *         xcairo_image_surface_get_argb32_data (cairo_surface_t *surface);
int
DLL_LOCAL                   xcairo_image_surface_get_argb32_stride (cairo_surface_t *surface);
DLL_LOCAL void xcairo_surface_mark_dirty (cairo_surface_t *surface);
DLL_LOCAL void xcairo_surface_flush (cairo_surface_t *surface);

DLL_LOCAL void        xg_object_unref (void *obj);

DLL_LOCAL GdkPixbuf * xgdk_pixbuf_new_from_file (const char *filename);
DLL_LOCAL gboolean    xgdk_pixbuf_is_argb32 (const GdkPixbuf *pb);
DLL_LOCAL gboolean    xgdk_pixbuf_is_xrgb32 (const GdkPixbuf *pb);
DLL_LOCAL void        xgdk_pixbuf_get_width_height_stride (const GdkPixbuf *pb,
                                                      int *widght,
                                                      int *height,
                                                      int *stride);
DLL_LOCAL guint32  *  xgdk_pixbuf_get_argb32_pixels   (GdkPixbuf *pb);

#if HAVE_LIBPULSE
void                xpa_context_connect_to_default_server (pa_context *c);
pa_context_state_t  xpa_context_get_state              (pa_context *c);
pa_context *        xpa_context_new_with_proplist      (pa_mainloop_api *mainloop,
                                                        const char *name,
                                                        pa_proplist *proplist);
void                xpa_context_set_state_callback     (pa_context *c,
							pa_context_notify_cb_t cb,
							void *userdata);
#ifdef USE_GLIB_MAINLOOP
pa_glib_mainloop *   xpa_glib_mainloop_new             (GMainContext* c);
#endif
void                xpa_mainloop_free                  (pa_mainloop *m);
pa_mainloop_api *   xpa_mainloop_get_api               (pa_mainloop *m);
void                xpa_mainloop_blocking_iterate      (pa_mainloop * m);
int                 xpa_mainloop_nonblocking_iterate   (pa_mainloop * m);
pa_mainloop *       xpa_mainloop_new                   (void);
void                xpa_proplist_free                  (pa_proplist *p);
pa_proplist *       xpa_proplist_new                   (void);
void                xpa_proplist_sets                  (pa_proplist *p,
                                                        const char *key,
                                                        const char *value);
void                xpa_stream_connect_playback_to_default_device (pa_stream *s,
                                                                   pa_context *c,
                                                                   const pa_buffer_attr *attr,
                                                                   pa_stream_flags_t flags);
pa_usec_t           xpa_stream_get_time                (pa_stream *s);

pa_stream *         xpa_stream_new_with_proplist       (pa_context *c,
							const char *name,
							const pa_sample_spec *ss,
							const pa_channel_map *map,
							pa_proplist *p);
void                xpa_stream_set_started_callback     (pa_stream *p,
							 pa_stream_notify_cb_t cb,
							 void *userdata);
void                xpa_stream_set_write_callback       (pa_stream *p,
							 pa_stream_request_cb_t cb,
							 void *userdata);
void                xpa_stream_write                   (pa_stream * p,
                                                        const void *data,
                                                        size_t nbytes);
#endif
#endif
