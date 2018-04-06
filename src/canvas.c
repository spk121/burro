#include <math.h>
#include <gtk/gtk.h>
#include <libguile.h>

#include "canvas.h"
#include "canvas_colors.h"
#include "canvas_bg.h"
#include "canvas_lib.h"

struct _BurroCanvas
{
    GtkDrawingArea parent;
    guint32        backdrop;
    gboolean       blank_flag;
    gboolean       colorswap_flag;
    gdouble        brightness;

    cairo_t *context;
    cairo_surface_t *surface;
    gboolean dirty;

    PangoLayout *layout;
    gboolean layout_flag;
    int layout_priority;
    guint32 font_color;

    guint tick_cb_id;
};

static void draw ();

G_DEFINE_TYPE(BurroCanvas, burro_canvas, GTK_TYPE_DRAWING_AREA)

BurroCanvas *canvas_cur = NULL;

static gboolean
canvas_draw (GtkWidget *widget, cairo_t *cr)
{
    BurroCanvas *canvas = BURRO_CANVAS(widget);

    if (canvas->surface)
    {
        cairo_set_source_surface (cr, canvas->surface, 0, 0);
        cairo_paint (cr);
    }
    return FALSE;
}

static int
tick_cb (GtkWidget *widget, GdkFrameClock *frame_clock, void *user_data)
{
    if (gtk_widget_is_visible (widget))
    {
        if (canvas_cur->dirty
            || canvas_bg_is_dirty(0) || canvas_bg_is_dirty(1)
            || canvas_bg_is_dirty(2) || canvas_bg_is_dirty(3))
        {
            // Draw it up on a backbuffer
            draw();

            // Invalidate the widget so that the "draw" signal get
            // called, which is where we actually blit it to the
            // widget.
            gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

            canvas_bg_set_clean(0);
            canvas_bg_set_clean(1);
            canvas_bg_set_clean(2);
            canvas_bg_set_clean(3);
            canvas_cur->dirty = FALSE;
        }
    }
    else
    {
        //audio_pause ();
        // Figure out a way to sleep until the next gtk event comes in
    }
    return TRUE;
}

static void draw_backdrop_color ()
{
    double r = 0.0, g = 0.0, b = 0.0;
    guint32 c32 = canvas_cur->backdrop;
    r = ((double)((c32 & 0x00ff0000) >> 16)) / 255.0;
    g = ((double)((c32 & 0x0000ff00) >> 8)) / 255.0;
    b = ((double)((c32 & 0x000000ff))) / 255.0;
    if (canvas_cur->colorswap_flag)
    {
        double tmp = r;
        r = b;
        b = tmp;
    }
    r = CLAMP(r * canvas_cur->brightness, 0.0, 1.0); 
    g = CLAMP(g * canvas_cur->brightness, 0.0, 1.0); 
    b = CLAMP(b * canvas_cur->brightness, 0.0, 1.0); 
    cairo_set_source_rgb (canvas_cur->context, r, g, b);
    cairo_paint (canvas_cur->context);
}

static void compute_transform (cairo_matrix_t *matrix,
                               double rotation_center_screen_x,
                               double rotation_center_screen_y,
                               double rotation_center_bitmap_row,
                               double rotation_center_bitmap_column,
                               double rotation_angle, double expansion_factor)
{
    double xx, xy, yx, yy, x0, y0;
    double sn, cs;
    if (expansion_factor == 0.0)
        expansion_factor = 1.0;
    sn = sin (rotation_angle);
    cs = cos (rotation_angle);
    xx = expansion_factor * cs;
    xy = expansion_factor * sn;
    yx = -xy;
    yy = xx;
    x0 = (rotation_center_screen_x
          - (xx * (double)rotation_center_bitmap_column
             + xy * (double) rotation_center_bitmap_row));
    y0 = (rotation_center_screen_y
          - (yx * (double)rotation_center_bitmap_column
             + yy * (double) rotation_center_bitmap_row));
    matrix->xx = xx;
    matrix->xy = xy;
    matrix->yx = yx;
    matrix->yy = yy;
    matrix->x0 = x0;
    matrix->y0 = y0;
}

static void paint_transformed_image (cairo_t *context,
                                     cairo_matrix_t *matrix,
                                     cairo_surface_t *surface)
{
    /* Set the coordinate transform */
    cairo_set_matrix (context, matrix);

    /* Now copy it to the screen */
    cairo_set_source_surface (context, surface, CANVAS_MARGIN, CANVAS_MARGIN);
    cairo_paint (context);

    /* Restore the coordinate system to normal */
    cairo_identity_matrix (context);
}


static void draw_background_layer (bg_index_t layer)
{
    cairo_surface_t *surf;
    cairo_matrix_t matrix;
    double scroll_x, scroll_y, rotation_center_x, rotation_center_y;
    double rotation, expansion;

    surf = canvas_bg_get_cairo_surface (layer);
    cairo_surface_mark_dirty (surf);
    canvas_bg_get_transform (layer, &scroll_x, &scroll_y,
                             &rotation_center_x, &rotation_center_y,
                             &rotation, &expansion);
    compute_transform (&matrix, scroll_x, scroll_y,
                       rotation_center_x, rotation_center_y,
                       rotation, expansion);
    paint_transformed_image (canvas_cur->context, &matrix, surf);
    // xcairo_surface_destroy (surf);
}

static void draw_textbox()
{
    cairo_save (canvas_cur->context);
    double r = 0.0, g = 0.0, b = 0.0;
    guint32 c32 = canvas_cur->font_color;
    r = ((double)((c32 & 0x00ff0000) >> 16)) / 255.0;
    g = ((double)((c32 & 0x0000ff00) >> 8)) / 255.0;
    b = ((double)((c32 & 0x000000ff))) / 255.0;
    if (canvas_cur->colorswap_flag)
    {
        double tmp = r;
        r = b;
        b = tmp;
    }
    r = CLAMP(r * canvas_cur->brightness, 0.0, 1.0); 
    g = CLAMP(g * canvas_cur->brightness, 0.0, 1.0); 
    b = CLAMP(b * canvas_cur->brightness, 0.0, 1.0); 
    cairo_set_source_rgb (canvas_cur->context, r, g, b);
    
    cairo_move_to(canvas_cur->context, CANVAS_MARGIN, CANVAS_MARGIN);
    pango_cairo_show_layout (canvas_cur->context, canvas_cur->layout);
    cairo_restore(canvas_cur->context);
}


static void draw ()
{
    draw_backdrop_color ();
    if (canvas_cur->blank_flag)
        goto end_draw;

    for (int z = CANVAS_ZLEVEL_COUNT - 1; z >= 0; z --)
    {
        if (canvas_bg_is_shown (z))
            draw_background_layer (z);
    }
#if 0
    for (int priority = PRIORITY_COUNT - 1; priority >= 0; priority --)
    {
        SCM obj_display_list = scm_variable_ref(G_obj_display_list);
        for (int id = scm_to_int (scm_length (obj_display_list)) - 1; id >= 0; id --)
        {
            SCM obj = scm_list_ref (obj_display_list, scm_from_int (id));
            if (obj_get_priority (obj) == priority)
                draw_obj (obj);
        }
        SCM textbox = scm_variable_ref (G_textbox_var);
    }
#endif

    for (int priority = CANVAS_ZLEVEL_COUNT - 1; priority >= 0; priority --)
    {
        if (canvas_cur->layout_flag && (canvas_cur->layout_priority == priority))
            draw_textbox ();
    }

end_draw:

    //if (console_is_visible ())
    //    draw_console_layer ();
    cairo_surface_mark_dirty (canvas_cur->surface);
}


static void
burro_canvas_init (BurroCanvas *win)
{
    gtk_widget_set_events (GTK_WIDGET(win),
                           GDK_EXPOSURE_MASK
                           | GDK_LEAVE_NOTIFY_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_POINTER_MOTION_MASK);

    gtk_widget_set_size_request(GTK_WIDGET(win),
                                CANVAS_WIDTH + 2 * CANVAS_MARGIN,
                                CANVAS_HEIGHT + 2 * CANVAS_MARGIN);

    win->blank_flag = FALSE;
    win->colorswap_flag = FALSE;
    win->brightness = 1.0;

    win->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                               CANVAS_WIDTH + 2 * CANVAS_MARGIN,
                                               CANVAS_HEIGHT + 2 * CANVAS_MARGIN);
    win->context = cairo_create (win->surface);
    cairo_set_antialias (win->context, CAIRO_ANTIALIAS_NONE);

    // Set up the background
    canvas_bg_init ();
    
    // Set up the text drawing
    win->layout = pango_cairo_create_layout (win->context);
    win->layout_flag = FALSE;
    win->layout_priority = 0;
    win->font_color = 0xB2B2B2;

    /* Set the font. */
    PangoFontDescription *desc = pango_font_description_from_string("Serif 16");
    pango_layout_set_font_description (win->layout, desc);
    pango_font_description_free (desc);

    /* Set up word wrapping. */
    pango_layout_set_width (win->layout, CANVAS_WIDTH * PANGO_SCALE);
    pango_layout_set_height (win->layout, CANVAS_HEIGHT * PANGO_SCALE);
    pango_layout_set_wrap (win->layout, PANGO_WRAP_WORD_CHAR);
    
    win->tick_cb_id = gtk_widget_add_tick_callback (GTK_WIDGET(win),
                                                    tick_cb,
                                                    NULL,
                                                    NULL);

    // Start up the audio engine
    canvas_audio_init();
    
    win->dirty = TRUE;
    canvas_cur = win;
}

// Do I need delete here? The docs say "Signal emitted if a user
// requests that a toplevel window is closed".
static gboolean
canvas_delete (GtkWidget *widget, GdkEventAny *eveny)
{
    gtk_widget_destroy (widget);
    return TRUE;
}

static void
canvas_dispose (GObject *object)
{
    BurroCanvas *win;

    canvas_audio_fini();
    canvas_bg_fini();
    
    win = BURRO_CANVAS (object);
    if (win->tick_cb_id > 0)
    {
        gtk_widget_remove_tick_callback (GTK_WIDGET(win), win->tick_cb_id);
        win->tick_cb_id = 0;
    }
    g_clear_object(&(win->layout));
    win->layout_flag = FALSE;

    if (win->context)
    {
        cairo_destroy (win->context);
        win->context = NULL;
    }
    
    if (win->surface)
    {
        unsigned int rc = cairo_surface_get_reference_count (win->surface);
        if (cairo_surface_get_reference_count (win->surface) != 1)
            g_warning ("in canvas_dispose, win->surface reference count is %d != 1\n", rc);
        cairo_surface_destroy (win->surface);
        if (rc == 1)
            win->surface = NULL;
    }
    G_OBJECT_CLASS (burro_canvas_parent_class)->dispose (object);
}

static void
burro_canvas_class_init (BurroCanvasClass *class)
{
    GtkWidgetClass *gtkclass = GTK_WIDGET_CLASS (class);
    GObjectClass *gclass = G_OBJECT_CLASS(class);

    gclass->dispose = canvas_dispose;

    /* basics */
    gtkclass->draw = canvas_draw;
    
    /* events */
    gtkclass->delete_event = canvas_delete;
    
}

BurroCanvas *
burro_canvas_new ()
{
    return g_object_new (BURRO_TYPE_CANVAS, NULL);
}

cairo_t *
get_canvas_context_cur()
{
    return canvas_cur->context;
}


SCM_DEFINE(G_canvas_set_blank, "set-blank", 1, 0, 0, (SCM flag), "\
Given a FLAG, this sets the canvas's blank parameter.  When blank is\n\
#t, the canvas is drawn.  When it is #f, it is just the background\n\
color.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);
    
    if (scm_is_true(flag))
        canvas_cur->blank_flag = TRUE;
    else
        canvas_cur->blank_flag = FALSE;

    canvas_cur->dirty = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_canvas_get_blank, "get-blank", 0, 0, 0, (void), "\
Returns the canvas's blank parameter.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);

    return scm_from_bool (canvas_cur->blank_flag);
}

SCM_DEFINE(G_canvas_set_colorswap, "set-colorswap", 1, 0, 0, (SCM flag), "\
Given a FLAG, this sets the canvas's colorswap parameter.  When\n\
colorswap is #t, the canvas is drawn with red and blue swapped.  When\n\
it is #f, is is draw normally.")
    
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);
    
    if (scm_is_true(flag))
        canvas_cur->colorswap_flag = TRUE;
    else
        canvas_cur->colorswap_flag = FALSE;
    canvas_cur->dirty = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_canvas_get_colorswap, "get-colorswap", 0, 0, 0, (void), "\
Returns the canvas's colorswap parameter.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);

    return scm_from_bool (canvas_cur->colorswap_flag);
}

SCM_DEFINE(G_canvas_set_brightness, "set-brightness", 1, 0, 0, (SCM val), "\
This adjusts the brightness of the canvas. Useful values are between\n\
0.1 (dark) and about 2.0 (probably saturated), where 1.0 is normal\n\
brightness.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);

    if (scm_is_real (val))
    {
        gdouble brightness = scm_to_double (val);
        if (brightness < 0.0)
            brightness = 0.0;
        canvas_cur->brightness = brightness;
    }
    canvas_cur->dirty = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_canvas_get_brightness, "get-brightness", 0, 0, 0, (void), "\
Returns the canvas's brightness parameter.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);
    
    return scm_from_double (canvas_cur->brightness);
}

SCM_DEFINE(G_canvas_set_backdrop, "set-backdrop", 1, 0, 0, (SCM s_color), "\
Given a 32-bit RGB colorval, this sets the canvas backdrop to that color.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);

    if (scm_is_string (s_color))
    {
        char *color = scm_to_utf8_string (s_color);
        guint32 colorval;
        gboolean found = canvas_lookup_colorval (color, &colorval);
        if (found)
        {
            canvas_cur->backdrop = colorval;
            canvas_cur->dirty = TRUE;
        }
    }
    else if (scm_is_unsigned_integer (s_color, 0, 0xFFFFFF))
    {
        canvas_cur->backdrop = scm_to_uint32 (s_color);
        canvas_cur->dirty = TRUE;
    }
    else
        scm_wrong_type_arg_msg("set-backdrop", SCM_ARG1, s_color, "color name or value");
    
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_canvas_get_backdrop, "get-backdrop", 0, 0, 0, (void), "\
Returns a 32-bit RGB color, which is the canvas backdrop color.")
{
    g_return_val_if_fail (canvas_cur != NULL, SCM_UNSPECIFIED);
    
    return scm_from_uint32 (canvas_cur->backdrop);
}

SCM_DEFINE (G_canvas_set_default_font, "set-font", 1, 0, 0,
            (SCM s_str), "")
{
    SCM_ASSERT (scm_is_string (s_str), s_str, SCM_ARG1, "set-font");

    char *str = scm_to_utf8_string (s_str);
    PangoFontDescription *desc = pango_font_description_from_string(str);
    pango_layout_set_font_description (canvas_cur->layout, desc);
    pango_font_description_free (desc);
    free (str);
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_canvas_set_default_font_color, "set-font-color", 1, 0, 0,
            (SCM s_str), "")
{
    SCM_ASSERT (scm_is_string (s_str), s_str, SCM_ARG1, "set-font");

    char *str = scm_to_utf8_string (s_str);
    guint32 colorval;
    gboolean ret = canvas_lookup_colorval (str, &colorval);
    free (str);
    if (ret)
        canvas_cur->font_color = colorval;
    return scm_from_bool (ret);
}

SCM_DEFINE(G_canvas_set_markup, "set-markup", 1, 0, 0, (SCM s_str), "\
Given a string, possibly with Pango-style XML markup, sets the\n\
textbox text.")
{
    SCM_ASSERT (scm_is_string (s_str), s_str, SCM_ARG1, "set-markup");
    char *str = scm_to_utf8_string (s_str);
    pango_layout_set_markup (canvas_cur->layout, str, -1);
    free (str);
    canvas_cur->layout_flag = TRUE;
    
    canvas_cur->dirty = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_canvas_update_text_fgcolor_on_region, "update-text-fgcolor-on-region",
           3, 0, 0, (SCM s_colorval, SCM begin, SCM end), "")
{
    guint32 colorval = scm_to_uint32 (s_colorval);
    int a = scm_to_int (begin);
    int b = scm_to_int (end);
    guint16 red, green, blue;

    red = (((colorval & 0xFF0000) >> 16) << 8);
    green = (((colorval & 0xFF00) >> 8) << 8);
    blue = (colorval & 0xFF) << 8;

    PangoAttrList *attrs = pango_layout_get_attributes (canvas_cur->layout);
    PangoAttribute *fgcolor = pango_attr_foreground_new(red, green, blue);
    // FIXME: am I likely confusing UTF-8 bytes and codepoints here?
    fgcolor->start_index = a;
    fgcolor->end_index = b;
    pango_attr_list_change (attrs, fgcolor);
    pango_layout_set_attributes (canvas_cur->layout, attrs);
    // g_object_unref (attrs);
    canvas_cur->dirty = TRUE;
    return SCM_UNSPECIFIED;
}

gboolean
canvas_xy_to_index (BurroCanvas *canvas,
                    double x, double y,
                    int *index, int *trailing)
{
    gboolean ret;

    x -= CANVAS_MARGIN;
    y -= CANVAS_MARGIN;
    x *=  PANGO_SCALE;
    y *=  PANGO_SCALE;
    ret = pango_layout_xy_to_index (canvas_cur->layout, x, y, index, trailing);
    if (!ret)
        return FALSE;

    /* The UTF-8 index needs to be converted into UTF32 index. */
    const char *str = pango_layout_get_text (canvas_cur->layout);
    int i = 0;
    int offset = 0;
    while (i < *index)
    {
        i = g_utf8_next_char(str + i) - str;
        offset++;
    }
    *index = offset;
    return TRUE;
}

SCM_DEFINE(G_canvas_position_to_string_index,
           "position-to-string-index", 2, 0, 0, (SCM s_x, SCM s_y), "\
Converts a location in pixel coordinates to a codepoint index of a\n\
textbox's text. Returns #f if the position is not near a character,\n\
or an codepoint index otherwise.")
{
    int x = scm_to_double (s_x) * PANGO_SCALE;
    int y = scm_to_double (s_y) * PANGO_SCALE;
    int index, trailing;
    gboolean ret = pango_layout_xy_to_index (canvas_cur->layout, x, y, &index, &trailing);
    if (!ret)
        return SCM_BOOL_F;

    /* The UTF-8 index needs to be converted into UTF32 index. */
    const char *str = pango_layout_get_text (canvas_cur->layout);
    int i = 0;
    int offset = 0;
    while (i < index)
    {
        i = g_utf8_next_char(str + i) - str;
        offset++;
    }
    
    //return scm_list_2 (scm_from_int (offset), scm_from_int (trailing));
    return scm_from_int(offset);
}


void
canvas_init_guile_procedures ()
{
    canvas_vram_init_guile_procedures ();
    canvas_bg_init_guile_procedures ();
    canvas_audio_init_guile_procedures();
#ifndef SCM_MAGIC_SNARFER
#include "canvas.x"
#endif
    scm_c_export ("set-blank",
                  "get-blank",
                  "set-colorswap",
                  "get-colorswap",
                  "set-brightness",
                  "get-brightness",
                  "set-backdrop",
                  "get-backdrop",
                  "set-markup",
                  "position-to-string-index",
                  "update-text-fgcolor-on-region",
                  "set-font",
                  "set-font-color",
                  NULL);
}

/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/

