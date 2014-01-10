#include "../x/xgdk.h"
#include "../x/xglib.h"
#include "../x/xgtk.h"
#include "draw.h"
#include "eng.h"
#include "loop.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <stdlib.h>

GtkWidget *window;
GtkWidget *fixed;
GtkWidget *main_screen;
GtkWidget *sub_screen;
gulong destroy_signal_id;
gulong key_press_event_signal_id;
gulong key_release_event_signal_id;
gulong window_state_event_signal_id;
gboolean blank_flag = FALSE;
gboolean colorswap_flag = FALSE;
gdouble brightness = 1.0;

static void destroy_cb(GtkWidget* widget, gpointer dummy);

gboolean
eng_is_blank ()
{
  return blank_flag;
}

void
eng_blank ()
{
  blank_flag = TRUE;
}

void
eng_unblank ()
{
  blank_flag = FALSE;
}

gboolean 
eng_is_colorswap ()
{
  return colorswap_flag;
}

void
eng_colorswap ()
{
  colorswap_flag = TRUE;
}

void
eng_uncolorswap ()
{
  colorswap_flag = FALSE;
}

double
eng_get_brightness ()
{
  return brightness;
}

void
eng_set_brightness (gdouble b)
{
  brightness = b;
}

GtkWidget *eng_initialize ()
{
  window = xgtk_window_new (GTK_WINDOW_TOPLEVEL);
  xgtk_container_set_border_width (GTK_CONTAINER (window), 8);
  xgtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  xgtk_window_set_title(GTK_WINDOW(window), "Project Burro Engine");
  
  /* Set GNOME properties that PulseAudio likes to have */
  xg_set_application_name("APPLICATION NAME");
  // gtk_window_set_default_icon_name(title);
  xg_setenv("PULSE_PROP_media.role", "game", TRUE);
  
  xgtk_widget_realize (window);
  
  fixed = xgtk_fixed_new ();
  xgtk_container_add (GTK_CONTAINER (window), fixed);
  
  sub_screen = xgtk_drawing_area_new();
  xgtk_widget_set_size_request (sub_screen,
				SUB_SCREEN_WIDTH * SUB_SCREEN_MAGNIFICATION,
				SUB_SCREEN_HEIGHT * SUB_SCREEN_MAGNIFICATION);
  
  xgtk_fixed_put(GTK_FIXED(fixed), sub_screen, 0, 0);
  
  main_screen = xgtk_drawing_area_new();
  xgtk_widget_set_size_request(main_screen,
			       MAIN_SCREEN_WIDTH * MAIN_SCREEN_MAGNIFICATION,
			       MAIN_SCREEN_HEIGHT * MAIN_SCREEN_MAGNIFICATION);
  
  xgtk_fixed_put(GTK_FIXED(fixed),
		 main_screen,
		 0,
		 SUB_SCREEN_HEIGHT * SUB_SCREEN_MAGNIFICATION + 10);
  
  
  destroy_signal_id =  
      xg_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK (destroy_cb), NULL); 
  /* key_press_event_signal_id =  */
  /*     xg_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (key_event_cb), NULL); */
  /* key_release_event_signal_id =  */
  /*     xg_signal_connect (G_OBJECT (window), "key-release-event", G_CALLBACK (key_event_cb), NULL); */
  /* window_state_event_signal_id =  */
  /*     xg_signal_connect (GTK_WIDGET(window), "window-state-event", G_CALLBACK (window_state_event_cb), NULL); */
  return window;
}

static void destroy_cb (GtkWidget* widget, gpointer dummy)
{
  loop_quit ();
}

void eng_present()
{
    cairo_t *cr;

    /* Have the video view draw the video model onto the screen */

    cr = xgdk_cairo_create (xgtk_widget_get_window (main_screen));

    xcairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    xcairo_scale(cr, MAIN_SCREEN_MAGNIFICATION, MAIN_SCREEN_MAGNIFICATION);
    xcairo_set_source_surface (cr, draw_get_main_screen_surface (), 0, 0);
    // cairo_surface_write_to_png(e.priv.main_screen_surface, "burro_main_screen_present.png");
    xcairo_paint (cr);
    xcairo_destroy(cr);

    cr = xgdk_cairo_create (xgtk_widget_get_window (sub_screen));

    xcairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    xcairo_scale(cr, SUB_SCREEN_MAGNIFICATION, SUB_SCREEN_MAGNIFICATION);
    xcairo_set_source_surface (cr, draw_get_sub_screen_surface (), 0, 0);
    // cairo_surface_write_to_png(e.priv.sub_screen_surface, "burro_sub_screen_present.png");
    xcairo_paint (cr);
    xcairo_destroy(cr);
}