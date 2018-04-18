#include "canvas_lib.h"

static gboolean
is_valid_cairo_format_t (cairo_format_t f)
{
    return (f == CAIRO_FORMAT_ARGB32
            || f == CAIRO_FORMAT_RGB24
            || f == CAIRO_FORMAT_A8
            || f == CAIRO_FORMAT_A1
            || f == CAIRO_FORMAT_RGB16_565);
}

cairo_surface_t *
xcairo_image_surface_create (cairo_format_t format, int width, int height)
{
    cairo_surface_t *s;
    cairo_status_t status;
    g_return_val_if_fail (is_valid_cairo_format_t (format), NULL);
    g_return_val_if_fail (width > 0, NULL);
    g_return_val_if_fail (height > 0, NULL);
    s = cairo_image_surface_create (format, width, height);
    status = cairo_surface_status (s);
    if (status != CAIRO_STATUS_SUCCESS)
        g_critical ("cairo_image_surface_create was not successful");
    return s;
}

guint32 *
xcairo_image_surface_get_argb32_data (cairo_surface_t *surface)
{
    guint32 *data;
    g_return_val_if_fail (surface != NULL, NULL);
    g_return_val_if_fail (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32, NULL);
    g_return_val_if_fail (cairo_surface_get_reference_count (surface) > 0, NULL);
    data = (guint32 *) cairo_image_surface_get_data (surface);
    if (data == NULL)
        g_critical ("cairo_image_surface_get_data returned NULL");
    return data;
}

int
xcairo_image_surface_get_argb32_stride (cairo_surface_t *surface)
{
    int stride;
    g_return_val_if_fail (surface != NULL, 0);
    g_return_val_if_fail (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32, 0);
    g_return_val_if_fail (cairo_surface_get_reference_count (surface) > 0, 0);
    stride = cairo_image_surface_get_stride (surface);
    if (stride <= 0)
        g_critical ("cairo_image_surface_get_stride returned invalid");
    return stride / (int) sizeof (guint32);
}
void
xcairo_surface_flush (cairo_surface_t *surface)
{
    cairo_status_t status;
    g_return_if_fail (surface != NULL);
    g_return_if_fail (cairo_surface_get_reference_count (surface) > 0);
    cairo_surface_flush (surface);
    status = cairo_surface_status (surface);
    if (status != CAIRO_STATUS_SUCCESS)
        g_critical ("cairo_surface_flush was not successful");
}

void
xcairo_surface_mark_dirty (cairo_surface_t *surface)
{
    cairo_status_t status;
    g_return_if_fail (surface != NULL);
    g_return_if_fail (cairo_surface_get_reference_count (surface) > 0);
    cairo_surface_mark_dirty (surface);
    status = cairo_surface_status (surface);
    if (status != CAIRO_STATUS_SUCCESS)
        g_critical ("cairo_surface_mark_dirty was not successful");
}

void
xg_object_unref (void *obj)
{
    g_return_if_fail (obj != NULL);
    g_object_unref (obj);
}

GdkPixbuf *
xgdk_pixbuf_new_from_file (const char *filename)
{
  GError *err = NULL;
  g_return_val_if_fail (filename != NULL, NULL);

  GdkPixbuf *pb;
  pb = gdk_pixbuf_new_from_file (filename, &err);
  if (pb == NULL)
    {
      g_critical ("gdk_pixbuf_new_from_file (%s) returned NULL: %s",
		  filename,
		  err->message);
      g_error_free (err);
    }
  return pb;
}

gboolean
xgdk_pixbuf_is_argb32 (const GdkPixbuf *pb)
{
  g_return_val_if_fail (pb != NULL, FALSE);
  
  return ((gdk_pixbuf_get_colorspace(pb) == GDK_COLORSPACE_RGB)
	  && (gdk_pixbuf_get_bits_per_sample(pb) == 8)
	  && (gdk_pixbuf_get_has_alpha(pb))
	  && (gdk_pixbuf_get_n_channels(pb) == 4));
}

gboolean
xgdk_pixbuf_is_xrgb32 (const GdkPixbuf *pb)
{
  g_return_val_if_fail (pb != NULL, FALSE);
  
  return ((gdk_pixbuf_get_colorspace(pb) == GDK_COLORSPACE_RGB)
          && (gdk_pixbuf_get_bits_per_sample(pb) == 8)
          && (!gdk_pixbuf_get_has_alpha(pb)));
}

void
xgdk_pixbuf_get_width_height_stride (const GdkPixbuf *pb,
				     int *width,
				     int *height,
				     int *stride)
{
  if (width != NULL)
    *width = 0;
  if (height != NULL)
    *height = 0;
  if (stride != NULL)
    *stride = 0;
  
  g_return_if_fail (pb != NULL);
  g_return_if_fail (xgdk_pixbuf_is_argb32 (pb) || xgdk_pixbuf_is_xrgb32 (pb));

  if (width != NULL)
    *width = gdk_pixbuf_get_width (pb);
  if (height != NULL)
      *height = gdk_pixbuf_get_height (pb);
  if (stride != NULL)
    *stride = gdk_pixbuf_get_rowstride (pb) / 4;
  
}

guint32 *
xgdk_pixbuf_get_argb32_pixels(GdkPixbuf *pb)
{
  g_return_val_if_fail (pb != NULL, NULL);
  g_return_val_if_fail (xgdk_pixbuf_is_argb32 (pb)
			|| xgdk_pixbuf_is_xrgb32 (pb), NULL);
  
  return (guint32 *) gdk_pixbuf_get_pixels (pb);
}

#if HAVE_LIBPULSE

static gboolean
is_valid_pa_context_state_t (pa_context_state_t c)
{
  return (c == PA_CONTEXT_UNCONNECTED
	  || c == PA_CONTEXT_CONNECTING
	  || c == PA_CONTEXT_AUTHORIZING
	  || c == PA_CONTEXT_SETTING_NAME
	  || c == PA_CONTEXT_READY
	  || c == PA_CONTEXT_FAILED
	  || c == PA_CONTEXT_TERMINATED);
}

void
xpa_context_connect_to_default_server (pa_context *c)
{
    g_return_if_fail (c != NULL);

    int ret;
    ret = pa_context_connect (c,
                              (const char *) NULL,
                              PA_CONTEXT_NOFLAGS,
                              (const pa_spawn_api *) NULL);
    if (ret != 0)
        g_critical ("pa_context_connect failed");
}

pa_context_state_t
xpa_context_get_state (pa_context *c)
{
  pa_context_state_t s;
  g_return_val_if_fail (c != NULL, PA_CONTEXT_UNCONNECTED);
  s = pa_context_get_state (c);
  g_return_val_if_fail (is_valid_pa_context_state_t (s), s);
  return s;
}

pa_context *
xpa_context_new_with_proplist (pa_mainloop_api *mainloop,
                               const char *name,
                               pa_proplist *proplist)
{
    g_return_val_if_fail (mainloop != NULL, NULL);
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (proplist != NULL, NULL);

    pa_context *c;
    c = pa_context_new_with_proplist (mainloop, name, proplist);
    if (c == NULL)
        g_critical ("pa_context_new_with_proplist returned NULL");
    return c;
}

void
xpa_context_set_state_callback (pa_context *c, pa_context_notify_cb_t cb, void *userdata)
{
  g_return_if_fail (c != NULL);
  g_return_if_fail (cb != NULL);
  pa_context_set_state_callback (c, cb, userdata);
}

#ifdef USE_GLIB_MAINLOOP
pa_glib_mainloop *
xpa_glib_mainloop_new (GMainContext *c)
{
    pa_glib_mainloop *loop;
    g_return_val_if_fail (c != NULL, NULL);
    loop = pa_glib_mainloop_new (c);
    if (loop == NULL)
        g_critical ("pa_glib_mainloop_new returned NULL");
    return loop;
}
#endif

pa_mainloop_api *
xpa_mainloop_get_api (pa_mainloop *m)
{
  pa_mainloop_api *api;
  g_return_val_if_fail (m != NULL, NULL);
  api = pa_mainloop_get_api (m);
  if (api == NULL)
    g_critical ("pa_mainloop_get_api returned NULL");
  return api;
}

void
xpa_mainloop_blocking_iterate (pa_mainloop * m)
{
  int ret;
  g_return_if_fail (m != NULL);
  ret = pa_mainloop_iterate (m, 1, NULL);
  if (ret < 0)
    g_critical ("pa_mainloop_iterate did not succeed");
}

int
xpa_mainloop_nonblocking_iterate (pa_mainloop * m)
{
  int ret;
  g_return_val_if_fail (m != NULL, -1);
  ret = pa_mainloop_iterate (m, 0, NULL);
  if (ret < 0)
    g_critical ("pa_mainloop_iterate did not succeed");
  return ret;
}


void
xpa_mainloop_free (pa_mainloop *m)
{
  g_return_if_fail (m != NULL);
  pa_mainloop_free (m);
}

pa_mainloop *
xpa_mainloop_new (void)
{
  pa_mainloop *ml = pa_mainloop_new ();
  if (ml == NULL)
    g_critical ("pa_mainloop_new returned NULL");
  return ml;
}

void
xpa_proplist_free (pa_proplist *p)
{
  g_return_if_fail (p != NULL);
  pa_proplist_free (p);
}

pa_proplist *
xpa_proplist_new (void)
{
  pa_proplist *p;
  p = pa_proplist_new ();
  if (p == NULL)
    g_critical ("pa_proplist_new returned NULL");
  return p;
}

void
xpa_proplist_sets (pa_proplist *p, const char *key, const char *value)
{
  int ret;
  ret = pa_proplist_sets (p, key, value);
  if (ret != 0)
    g_critical ("pa_proplist_sets failed");
}

void xpa_stream_connect_playback_to_default_device (pa_stream *s, pa_context *c,
                                                    const pa_buffer_attr *attr,
                                                    pa_stream_flags_t flags)
{
  int ret;
  g_return_if_fail (s != NULL);
  ret = pa_stream_connect_playback (s, NULL, attr, flags, NULL, NULL);
  if (ret != 0)
      g_critical ("pa_stream_connect_playback failed: %s",
                  pa_strerror (pa_context_errno(c)));
}

pa_usec_t
xpa_stream_get_time (pa_stream *s)
{
    int ret;
    pa_usec_t r_usec;

    g_return_val_if_fail (s != NULL, 0);
    ret = pa_stream_get_time (s, &r_usec);
    if (ret != 0)
    {
        g_critical ("pa_stream_get_time failed to return a valid time");
        return 0;
    }
    return r_usec;
}

pa_stream *
xpa_stream_new_with_proplist (pa_context *c, const char *name, const pa_sample_spec *ss,
			      const pa_channel_map *map, pa_proplist *p)
{
  pa_stream *s;
  g_return_val_if_fail (c != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (ss != NULL, NULL);
  g_return_val_if_fail (map != NULL, NULL);
  g_return_val_if_fail (p != NULL, NULL);
  s = pa_stream_new_with_proplist (c, name, ss, map, p);
  if (s == NULL)
      g_critical ("pa_stream_new_with_proplist returned NULL: %s",
                  pa_strerror (pa_context_errno(c)));
  return s;
}

void
xpa_stream_set_started_callback (pa_stream *p, pa_stream_notify_cb_t cb, void *userdata)
{
  g_return_if_fail (p != NULL);
  g_return_if_fail (cb != NULL);
  pa_stream_set_started_callback (p, cb, userdata);
}

void
xpa_stream_set_write_callback (pa_stream *p, pa_stream_request_cb_t cb, void *userdata)
{
  g_return_if_fail (p != NULL);
  g_return_if_fail (cb != NULL);
  pa_stream_set_write_callback (p, cb, userdata);
}

void
xpa_stream_write (pa_stream *p, const void *data, size_t nbytes)
{
    g_return_if_fail (p != NULL);
    g_return_if_fail (data != NULL);
    g_return_if_fail (nbytes > 0);

  int ret;
  ret = pa_stream_write (p, data, nbytes, NULL, 0, PA_SEEK_RELATIVE);
  if (ret != 0)
    g_critical ("pa_stream_write failed");
}

#endif
