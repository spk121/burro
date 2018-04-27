#include <gtk/gtk.h>
// #include <libguile.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "burro_app.h"
#include "burro_app_win.h"
#include "canvas.h"
#include "burro_resources.h"
#include "burro_lisp.h"
#include "burro_debug_window.h"
#include "burro_repl.h"

#define GAME_LOOP_MINIMUM_PERIOD_SECONDS (1.0 / 60.0)
#define GAME_LOOP_IDEAL_PERIOD_SECONDS (1.0 / 30.0)
#define GAME_LOOP_MAXIMUM_PERIOD_SECONDS (0.1)
#define __maybe_unused __attribute__((unused))
struct _BurroAppWindow
{
    GtkApplicationWindow parent;
    GtkAccelGroup *accels;
    BurroCanvas *canvas;
    // GtkMenuButton *gears;
    BurroDebugWindow *debug_window;

    // The main game loop is in the idle handler.
    gboolean minimized_flag;
    gboolean maximized_flag;
    guint game_loop_callback_id;
    gint game_loop_frame_count;
    gboolean game_loop_active_flag;
    gboolean game_loop_step_flag;
    gboolean game_loop_quitting;
    double game_loop_start_time;
    double game_loop_end_time;
    double game_loop_start_time_prev;
    gboolean game_loop_full_speed;
    double game_loop_avg_period;
    double game_loop_avg_duration;

    gboolean have_mouse_move_event;
    double mouse_move_x;
    double mouse_move_y;
    gboolean have_mouse_click_event;
    double mouse_click_x;
    double mouse_click_y;
    gboolean have_text_move_event;
    int text_move_location;
    gboolean have_text_click_event;
    int text_click_location;

    // Guile support
    SCM burro_module;           /* The (burro) module */
    SCM sandbox;                /* Opened files are parsed into this
                                 * anonymous module */
    char *sandbox_path;
    BurroRepl *repl;

    // Log handler
    guint log_handler_id;
};

static void signal_start_requested (GObject *obj, gpointer user_data);
static void signal_stop_requested (GObject *obj, gpointer user_data);
static void signal_step_requested (GObject *obj, gpointer user_data);
static void signal_repl_requested (GObject *obj, gpointer user_data);


#if 0
void
debug_peek_list_store_update(GtkListStore *list_store)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    gint i;
    const char *nullstr = "";

    for (i = VRAM_A; i < VRAM_COUNT; i ++)
    {
        path = gtk_tree_path_new_from_indices (i - VRAM_A, -1);
        gtk_tree_model_get_iter(GTK_TREE_MODEL (list_store), &iter, path);

        char *siz = vram_size_string(i);
        // gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            VRAM_COLUMN_NAME, vram_bank_name[i],
                            VRAM_COLUMN_TYPE, vram_type_name[vram_info[i].type],
                            VRAM_COLUMN_FILENAME, (vram_info[i].filename
                                              ? vram_info[i].filename
                                              : nullstr),
                            VRAM_COLUMN_SIZE, siz,
                            -1);
        g_free (siz);
    }
}
#endif

static void
action_open (GSimpleAction *simple __maybe_unused,
             GVariant      *parameter __maybe_unused,
             gpointer       user_data)
{
    GtkWindow *parent_window = GTK_WINDOW(user_data);
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name (filter, "Burro games");
    gtk_file_filter_add_pattern(filter, "*.burro");

    dialog = gtk_file_chooser_dialog_new ("Load File",
                                          parent_window,
                                          action,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Open",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog),
                                 filter);

    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        GFile *file = gtk_file_chooser_get_file (chooser);
        gtk_widget_destroy (dialog);
        burro_app_window_open (BURRO_APP_WINDOW (parent_window), file);
        g_object_unref (file);
    }
    else
        gtk_widget_destroy (dialog);
}

static void
action_view_tools (GSimpleAction *simple __maybe_unused,
                   GVariant      *parameter __maybe_unused,
                   gpointer       user_data)
{
    BurroAppWindow *window;
    gboolean visible;

    window = BURRO_APP_WINDOW (user_data);

    if (!window->debug_window)
    {
        window->debug_window = burro_debug_window_new(window);
        gtk_window_set_destroy_with_parent (GTK_WINDOW(window->debug_window), TRUE);
        g_signal_connect(window->debug_window,
                         "start-requested",
                         G_CALLBACK(signal_start_requested),
                         window);
        g_signal_connect(window->debug_window,
                         "stop-requested",
                         G_CALLBACK(signal_stop_requested),
                         window);
        g_signal_connect(window->debug_window,
                         "step-requested",
                         G_CALLBACK(signal_step_requested),
                         window);
        g_signal_connect(window->debug_window,
                         "repl-requested",
                         G_CALLBACK(signal_repl_requested),
                         window);
        gtk_widget_show_all (GTK_WIDGET (window->debug_window));
    }
    else
    {
        visible = gtk_widget_is_visible (GTK_WIDGET (window->debug_window));
        gtk_widget_set_visible (GTK_WIDGET (window->debug_window), !visible);
    }
}

static GActionEntry win_entries[] =
{
    /* Stateless actions. */
    {"open",  action_open,  NULL, NULL, NULL, {0,0,0}},
    {"view-tools", action_view_tools, NULL, NULL, NULL, {0,0,0}},
};

static gboolean
accel_action_view_tools (GtkAccelGroup *accel_group __maybe_unused,
                         GObject *acceleratable,
                         guint keyval __maybe_unused,
                         GdkModifierType modifier __maybe_unused)
{
    BurroAppWindow *window;
    gboolean visible;

    window = BURRO_APP_WINDOW (acceleratable);

    if (!window->debug_window)
    {
        window->debug_window = burro_debug_window_new(window);
        gtk_widget_show_all (GTK_WIDGET (window->debug_window));
    }
    else
    {
        visible = gtk_widget_is_visible (GTK_WIDGET (window->debug_window));
        gtk_widget_set_visible (GTK_WIDGET (window->debug_window), !visible);
    }
    return TRUE;
}

G_DEFINE_TYPE(BurroAppWindow, burro_app_window, GTK_TYPE_APPLICATION_WINDOW);

static BurroAppWindow *app_window_cur;

static gboolean
signal_action_canvas_button_press_event (GtkWidget *widget __maybe_unused,
                                         GdkEventButton *event,
                                         gpointer user_data __maybe_unused)
{
    GdkEventButton *button = event;

    if (button->type == GDK_BUTTON_PRESS)
    {
        app_window_cur->have_mouse_click_event = TRUE;
        app_window_cur->mouse_click_x = button->x;
        app_window_cur->mouse_click_y = button->y;
        int index, trailing;
        if (canvas_xy_to_index (app_window_cur->canvas,
                                button->x,
                                button->y,
                                &index,
                                &trailing))
        {
            app_window_cur->have_text_click_event = TRUE;
            app_window_cur->text_click_location = index;
        }
    }

    return TRUE;
}

static gboolean
signal_action_canvas_motion_notify_event (GtkWidget *widget __maybe_unused,
                                          GdkEventMotion *event,
                                          gpointer user_data __maybe_unused)
{
    int index, trailing;

    app_window_cur->have_mouse_move_event = TRUE;
    app_window_cur->mouse_move_x = event->x;
    app_window_cur->mouse_move_y = event->y;
    if (canvas_xy_to_index (app_window_cur->canvas,
                            event->x,
                            event->y,
                            &index,
                            &trailing))
    {
        app_window_cur->have_text_move_event = TRUE;
        app_window_cur->text_move_location = index;
    }
    else
    {
        app_window_cur->have_text_move_event = TRUE;
        app_window_cur->text_move_location = -1;
    }

    return TRUE;
}

static void
signal_start_requested (GObject *obj __maybe_unused,
                        gpointer user_data)
{
    BurroAppWindow *win = user_data;
    win->game_loop_active_flag = TRUE;
    win->game_loop_step_flag = FALSE;
}

static void
signal_stop_requested (GObject *obj __maybe_unused,
                       gpointer user_data)
{
    BurroAppWindow *win = user_data;
    win->game_loop_active_flag = FALSE;
    win->game_loop_step_flag = FALSE;
}

static void
signal_step_requested (GObject *obj __maybe_unused,
                       gpointer user_data)
{
    BurroAppWindow *win = user_data;
    win->game_loop_active_flag = FALSE;
    win->game_loop_step_flag = TRUE;
}

static void
signal_repl_requested (GObject *obj __maybe_unused,
                       gpointer user_data)
{
    BurroAppWindow *win = user_data;
    burro_repl_enable (win->repl);
}

static SCM
call_pm_set_mouse_move (void *_win)
{
    BurroAppWindow *win = BURRO_APP_WINDOW (_win);

    scm_call_2 (scm_c_public_ref ("burro pm", "pm-set-mouse-move"),
                scm_from_double (win->mouse_move_x),
                scm_from_double (win->mouse_move_y));
    return SCM_BOOL_T;
}

static SCM
call_pm_set_mouse_click (void *_win)
{
    BurroAppWindow *win = BURRO_APP_WINDOW (_win);

    scm_call_2 (scm_c_public_ref ("burro pm", "pm-set-mouse-click"),
                scm_from_double (win->mouse_click_x),
                scm_from_double (win->mouse_click_y));
    return SCM_BOOL_T;
}

static SCM
call_pm_set_text_move (void *_win)
{
    BurroAppWindow *win = BURRO_APP_WINDOW (_win);
    scm_call_1 (scm_c_public_ref ("burro pm", "pm-set-text-move"),
                scm_from_int (win->text_move_location));
    return SCM_BOOL_T;
}

static SCM
call_pm_set_text_click (void *_win)
{
    BurroAppWindow *win = BURRO_APP_WINDOW (_win);
    scm_call_1 (scm_c_public_ref ("burro pm", "pm-set-text-click"),
                scm_from_int (win->text_click_location));
    return SCM_BOOL_T;
}

static SCM
call_pm_update (void *_pdt)
{
    double *pdt = _pdt;
    scm_call_1 (scm_c_public_ref ("burro pm", "pm-update"),
                scm_from_double(*pdt));
    return SCM_BOOL_T;
}


static gboolean
game_loop (gpointer user_data)
{
    if (g_source_is_destroyed (g_main_current_source()))
        return FALSE;

    BurroAppWindow *win = BURRO_APP_WINDOW (user_data);
    if (win->game_loop_quitting)
        return FALSE;


    if (!win->minimized_flag)
    {
        if (win->game_loop_active_flag || win->game_loop_step_flag)
        {
            win->game_loop_step_flag = FALSE;

            double start_time = g_get_monotonic_time() * 1.0e-6;
            if (win->game_loop_full_speed || ((start_time - win->game_loop_start_time) > GAME_LOOP_MINIMUM_PERIOD_SECONDS))
            {
                win->game_loop_start_time_prev = win->game_loop_start_time;
                win->game_loop_start_time = start_time;
                double dt = start_time - win->game_loop_start_time_prev;

                // If dt is too large, maybe we're resuming from a break point.
                // Frame-lock to the target rate for this frame.
                if (dt > GAME_LOOP_MAXIMUM_PERIOD_SECONDS)
                    dt = GAME_LOOP_IDEAL_PERIOD_SECONDS;

                // Update the audio engine
                canvas_audio_iterate();

                // Update the TCP repl, if it is running
                repl_tick(win->repl);

                // Pass any new mouse clicks and other events to the process manager
                if (win->have_mouse_move_event)
                {
                    scm_c_catch (SCM_BOOL_T,
                                 call_pm_set_mouse_move, win,
                                 default_error_handler,
                                 NULL, NULL, NULL);
                    win->have_mouse_move_event = FALSE;
                }
                if (win->have_mouse_click_event)
                {
                    g_message("sending mouse click to pm, %f %f", win->mouse_click_x, win->mouse_click_y);
                    scm_c_catch (SCM_BOOL_T,
                                 call_pm_set_mouse_click, win,
                                 default_error_handler,
                                 NULL, NULL, NULL);
                    win->have_mouse_click_event = FALSE;
                }
                if (win->have_text_move_event)
                {
                    // g_message("sending text move to pm, %d", win->text_move_location);
                    scm_c_catch (SCM_BOOL_T,
                                 call_pm_set_text_move, win,
                                 default_error_handler,
                                 NULL, NULL, NULL);
                    win->have_text_move_event = FALSE;
                }
                if (win->have_text_click_event)
                {
                    // g_message("sending text click to pm, %d", win->text_click_location);
                    scm_c_catch (SCM_BOOL_T,
                                 call_pm_set_text_click, win,
                                 default_error_handler,
                                 NULL, NULL, NULL);
                    win->have_text_click_event = FALSE;
                }

                // Let the guile processes run
                scm_c_catch (SCM_BOOL_T,
                             call_pm_update, &dt,
                             default_error_handler,
                             NULL, NULL, NULL);
                // Update subsystems

                // Render things

                // Keep some statistics on frame rate, and computation time
                win->game_loop_frame_count ++;
                win->game_loop_avg_period = (0.95 * win->game_loop_avg_period
                                             + 0.05 * dt);

                win->game_loop_end_time = g_get_monotonic_time() * 1.0e-6;
                win->game_loop_avg_duration = (0.95 * win->game_loop_avg_duration
                                               + 0.5 * (win->game_loop_end_time - win->game_loop_start_time));

                if (!(win->game_loop_frame_count % 60))
                {
                    char *update_rate_str = g_strdup_printf("%.1f", 1.0 / win->game_loop_avg_period);
                    char *duty_cycle_str = g_strdup_printf("%.1f%%", (100.0 * win->game_loop_avg_duration) / win->game_loop_avg_period);
                    if (win->debug_window)
                    {
                        burro_debug_window_update_rate_label (win->debug_window,
                                                              update_rate_str);
                        burro_debug_window_update_cycle_label (win->debug_window,
                                                               duty_cycle_str);
                    }
                    g_free (duty_cycle_str);
                    g_free (update_rate_str);
                }
            }
            else /* We are running too fast. */
                g_usleep(1000);
        }
        else /* ! active */
        {
            g_usleep (1000);
            // audio pause
            // sleep until next event
        }
    }
    else /* minimized */
        g_usleep (1000);

    return TRUE;
}

static void
timeout_action_destroy (gpointer data __maybe_unused)
{

}


static void
burro_app_window_init (BurroAppWindow *win)
{
    // We temporarily construct a canvas so that the canvas class is
    // ready to go before calling builder.
    BurroCanvas *tmp = burro_canvas_new();
    g_object_ref_sink (tmp);
    g_object_unref (tmp);

    gtk_widget_init_template (GTK_WIDGET (win));

    // Construct the menu
    g_action_map_add_action_entries (G_ACTION_MAP (win),
                                     win_entries, G_N_ELEMENTS (win_entries),
                                     win);

    GtkAccelGroup *accel = gtk_accel_group_new();
    GClosure *cl_view = g_cclosure_new(G_CALLBACK(accel_action_view_tools),
                                       win, NULL);
    gtk_accel_group_connect (accel,
                             GDK_KEY_F12,
                             0,
                             GTK_ACCEL_VISIBLE,
                             cl_view);
    gtk_window_add_accel_group(GTK_WINDOW (win),
                               accel);


    gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (win), TRUE);

    // Set the visibility of the text box
    win->burro_module = burro_lisp_new ();
    win->sandbox = SCM_BOOL_F;
    win->repl = burro_repl_new();

    // FIXME: how to do this properly
    // Really shrink the window down to minimal
#if 0
    gtk_window_resize (GTK_WINDOW(win),
                       CANVAS_WIDTH + 2 * CANVAS_MARGIN,
                       CANVAS_HEIGHT + 2 * CANVAS_MARGIN);
#endif

    // The game loop needs to know about mouse moves and mouse clicks
    // that specifically happen in the canvas.
    // This is for mouse clicks.
    win->have_mouse_click_event = FALSE;
    win->have_text_click_event = FALSE;
    g_signal_connect (G_OBJECT (win->canvas),
                      "button-press-event",
                      G_CALLBACK (signal_action_canvas_button_press_event),
                      NULL);
    // This is for mouse moves
    win->have_mouse_move_event = FALSE;
    win->have_text_move_event = FALSE;
    g_signal_connect (G_OBJECT (win->canvas),
                      "motion-notify-event",
                      G_CALLBACK (signal_action_canvas_motion_notify_event),
                      NULL);

    // Main Game Loop
    win->game_loop_active_flag = TRUE;
    win->game_loop_quitting = FALSE;
    win->game_loop_start_time = g_get_monotonic_time () * 1.0e-6;
    win->game_loop_end_time = g_get_monotonic_time () * 1.0e-6;
    win->game_loop_start_time_prev = win->game_loop_start_time - GAME_LOOP_IDEAL_PERIOD_SECONDS;
    win->game_loop_full_speed = FALSE;
    win->game_loop_frame_count = 0;
    win->game_loop_avg_period = GAME_LOOP_IDEAL_PERIOD_SECONDS;
    win->game_loop_avg_duration = GAME_LOOP_IDEAL_PERIOD_SECONDS / 4.0;

    win->game_loop_callback_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                       game_loop,
                                       (gpointer) win,
                                       timeout_action_destroy);


}

// This callback is called when the window is maximizing and
// minimizing.
gboolean
burro_app_window_state (GtkWidget *widget,
                        GdkEventWindowState *win_event)
{
    BurroAppWindow *win = BURRO_APP_WINDOW(widget);

    if (win_event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)
    {
        win->minimized_flag = TRUE;
        win->maximized_flag = FALSE;
    }
    else if (win_event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED
                                            | GDK_WINDOW_STATE_FULLSCREEN))
    {
        win->minimized_flag = FALSE;
        win->maximized_flag = TRUE;
    }
    else
    {
        win->minimized_flag = FALSE;
        win->maximized_flag = FALSE;
    }
    return TRUE;
}

// This callback is called when the application has asked this window
// to delete itself.
static  gboolean
burro_app_window_delete (GtkWidget       *widget,
                         GdkEventAny         *event __maybe_unused)
{
    gtk_widget_destroy(widget);
    return TRUE;
}

static void
burro_app_window_dispose (GObject *object)
{
    BurroAppWindow *win;
    win = BURRO_APP_WINDOW (object);

    win->game_loop_quitting = TRUE;
    if (win->game_loop_callback_id)
    {
        g_source_remove (win->game_loop_callback_id);
        win->game_loop_callback_id = 0;
    }

    if (win->canvas)
    {
        g_object_unref(win->canvas);
        win->canvas = NULL;
    }

    win->sandbox = SCM_BOOL_F;
    win->burro_module = SCM_BOOL_F;

    // g_clear_object (&win->repl);
    G_OBJECT_CLASS (burro_app_window_parent_class)->dispose (object);
}

static void
burro_app_window_class_init (BurroAppWindowClass *class)
{
    GObjectClass *gclass = G_OBJECT_CLASS(class);
    GtkWidgetClass *gtkclass = GTK_WIDGET_CLASS (class);

    gclass->dispose = burro_app_window_dispose;

    gtkclass->delete_event = burro_app_window_delete;
    gtkclass->window_state_event = burro_app_window_state;

    gtk_widget_class_set_template_from_resource (gtkclass, APP_WINDOW);

    gtk_widget_class_bind_template_child (gtkclass, BurroAppWindow, canvas);
    // gtk_widget_class_bind_template_child (gtkclass, BurroAppWindow, gears);
}

BurroAppWindow *
burro_app_window_new (BurroApp *app)
{
    app_window_cur = g_object_new (BURRO_APP_WINDOW_TYPE, "application", app, NULL);
    return app_window_cur;
}

void
burro_app_window_open (BurroAppWindow *win,
                       GFile *file)
{
    char *err_string;
    char *full;
    char *dir;

    if (file)
    {
        full = g_file_get_path (file);
        dir = g_path_get_dirname (full);
        g_debug("set vram path. full = %s dir = %s", full, dir);
        canvas_vram_set_path(dir);
        g_free (full);
        full = NULL;
    }

    win->sandbox = burro_make_sandbox (file, &err_string);
    if (file)
    {
        free (win->sandbox_path);
        win->sandbox_path = dir;
    }
    else
    {
        free (win->sandbox_path);
        win->sandbox_path = NULL;
        canvas_vram_set_path(NULL);
    }

    if (scm_is_false (win->sandbox))
    {
        char *filename = g_file_get_basename (file);

        GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
        GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (win),
                                         flags,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "There is a problem with “%s”.\n%s",
                                         filename,
                                         err_string);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        g_free (filename);
        free (err_string);
    }
    //if (file)
    //    g_object_unref (file);
}

char *
burro_app_window_eval_string_in_sandbox (const char *txt)
{
    SCM func = scm_c_public_ref("burro", "eval-string-in-sandbox");
    SCM ret = scm_call_2(func,
                         scm_from_utf8_string(txt),
                         app_window_cur->sandbox);
    char *output = scm_to_utf8_string (ret);
    return output;
}

gboolean
burro_app_window_get_active_flag()
{
    return app_window_cur->game_loop_active_flag;
}

void
burro_app_window_set_active_flag(gboolean x)
{
    app_window_cur->game_loop_active_flag = x;
}

gboolean
burro_app_window_get_step_flag()
{
    return app_window_cur->game_loop_step_flag;
}

void
burro_app_window_set_step_flag(gboolean x)
{
    app_window_cur->game_loop_step_flag = x;
}


SCM_DEFINE (G_burro_app_win_get_sandbox, "get-sandbox", 0, 0, 0,
            (void), "\
Return the current sandbox.")
{
    return app_window_cur->sandbox;
}

SCM_DEFINE (G_burro_app_win_set_title, "set-title", 1, 0, 0,
            (SCM title), "\
Set the title in titlebar of the main window.")
{
    if (scm_is_string (title))
    {
        char *str = scm_to_utf8_string(title);
        gtk_window_set_title (GTK_WINDOW(app_window_cur), str);
        free (str);
    }
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_burro_stop, "stop", 0, 0, 0, (void), "\
quit the program")
{
    gtk_main_quit();
}

void
burro_app_win_init_guile_procedures ()
{
#ifndef SCM_MAGIC_SNARFER
#include "burro_app_win.x"
#endif
    scm_c_export ("get-sandbox",
                  "set-title",
                  "stop",
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
