#ifndef BURRO_DEBUG_WINDOW_H
#define BURRO_DEBUG_WINDOW_H

#include "burro_app_win.h"

#include <gtk/gtk.h>

#define BURRO_DEBUG_WINDOW_TYPE (burro_debug_window_get_type ())
G_DECLARE_FINAL_TYPE (BurroDebugWindow, burro_debug_window, BURRO, DEBUG_WINDOW, GtkWindow)

BurroDebugWindow *burro_debug_window_new (BurroAppWindow *parent);
void              burro_debug_window_update_rate_label (BurroDebugWindow *win, const char *str);
void              burro_debug_window_update_cycle_label (BurroDebugWindow *win, const char *str);
void              burro_debug_window_log_string (BurroDebugWindow *win, GLogLevelFlags log_level, const char *str);
void burro_debug_window_init_guile_procedures ();

#endif
