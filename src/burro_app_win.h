#ifndef BURRO_APP_WIN_H
#define BURRO_APP_WIN_H

#include <gtk/gtk.h>
#include <cairo.h>
#include "burro_app.h"

#define BURRO_APP_WINDOW_TYPE (burro_app_window_get_type ())
G_DECLARE_FINAL_TYPE (BurroAppWindow, burro_app_window, BURRO, APP_WINDOW, GtkApplicationWindow)

BurroAppWindow *burro_app_window_new (BurroApp *app);
void     burro_app_window_open (BurroAppWindow *win,
			    GFile *file);

char *   burro_app_window_eval_string_in_sandbox (const char *str);
gboolean burro_app_window_get_active_flag();
void     burro_app_window_set_active_flag(gboolean x);
gboolean burro_app_window_get_step_flag();
void     burro_app_window_set_step_flag(gboolean x);


void burro_app_win_init_guile_procedures (void);
#endif
