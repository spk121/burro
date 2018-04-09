#include "burro_debug_window.h"
#include "canvas.h"
#include "burro_repl.h"
#include "burro_resources.h"
#include "burro_lineedit.h"
#include "burro_console.h"
#include "burro_journal.h"

#include <gtk/gtk.h>
#include <libguile.h>

#define __maybe_unused __attribute__((unused))

// Signals
enum {
    STOP_REQUEST,
    START_REQUEST,
    STEP_REQUEST,
    REPL_REQUEST,
    LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];
enum {
  PROP_0,
  NUM_PROPERTIES,
};


enum {
    DEBUG_COLUMN_TIME,                /* The name of the bank, like "VRAM A" */
    DEBUG_COLUMN_NAME,
    DEBUG_COLUMN_VALUE,
    DEBUG_COLUMN_STACK,
    DEBUG_N_COLUMNS
};


struct _BurroDebugWindow
{
    GtkWindow parent;
    GtkAccelGroup *accels;
    GtkDrawingArea *terminal;
    GtkTreeView *message_view;
    GtkScrolledWindow *message_scrolled_window;
    GtkComboBoxText *repl_combobox;
    GtkTreeView *vram_tree_view;
    GtkListStore *vram_list_store;
    GtkLabel *update_rate_label;
    GtkLabel *duty_cycle_label;
    GtkToggleButton *pause_button;
    GtkButton *next_button;
    GtkButton *refresh_button;
    GtkListStore *debug_peek_list_store;
    GtkTreeView *debug_peek_tree_view;

  // Log handler
    guint log_handler_id;
    guint key_press_event_signal_id;
    guint terminal_draw_event_id;
    guint terminal_timeout;
};

extern char *
burro_app_window_eval_string_in_sandbox (const char *txt);

#if 0
static GActionEntry win_entries[] =
{
    /* Stateless actions. */

};
#endif


static void
signal_action_repl_enabled (GtkComboBox *widget,
                            gpointer user_data)
{
    BurroDebugWindow *win = user_data;
    gint enabled = gtk_combo_box_get_active (widget);
    if (enabled)
    {
        g_signal_emit (win, signals[REPL_REQUEST], 0);
        gtk_widget_set_sensitive (GTK_WIDGET(widget), FALSE);
    }
}

G_DEFINE_TYPE(BurroDebugWindow, burro_debug_window, GTK_TYPE_WINDOW)

static BurroDebugWindow *debug_window_cur;

static void
signal_debug_pause (GtkToggleButton *button, gpointer user_data)
{
    BurroDebugWindow *win = BURRO_DEBUG_WINDOW(user_data);

    gboolean active = gtk_toggle_button_get_active (button);
    if (!active)
    {
        gtk_widget_set_sensitive (GTK_WIDGET(win->next_button), FALSE);
        g_signal_emit (win, signals[START_REQUEST], 0);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET(win->next_button), TRUE);
        gtk_label_set_text (win->update_rate_label, "paused");
        g_signal_emit (win, signals[STOP_REQUEST], 0);
    }
}

static void
signal_debug_next (GtkButton *button __maybe_unused,
                   gpointer user_data)
{
    BurroDebugWindow *win = BURRO_DEBUG_WINDOW(user_data);
    gboolean active = gtk_toggle_button_get_active (win->pause_button);
    if (!active)
        g_signal_emit (win, signals[STEP_REQUEST], 0);

}

char *
guile_any_to_c_string (SCM x)
{
    if (SCM_UNBNDP (x))
        return (g_strdup ("(undefined)"));
    else if (x == SCM_EOL)
        return (g_strdup ("(eol)" ));
    else if (x == SCM_EOF_VAL)
        return (g_strdup ("(eof)"));
    else if (x == SCM_UNSPECIFIED)
        return NULL;
    else if (scm_is_bool (x)) {
        if (scm_is_true (x))
            return g_strdup("#t");
        else
            return g_strdup("#f");
    }
    else {
        SCM proc = scm_c_eval_string ("display");
        SCM outp = scm_open_output_string ();
        scm_call_2 (proc, x, outp);
        SCM ret = scm_get_output_string (outp);
        scm_close (outp);
        return scm_to_locale_string (ret);
    }
}

static GtkListStore *debug_peek_list_store_new()
{
    GtkListStore *list_store;

    list_store = gtk_list_store_new (DEBUG_N_COLUMNS,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING);
    return list_store;
}               

static gboolean key_event_terminal (GtkWidget *widget __maybe_unused,
                                    GdkEventKey *event,
                                    gpointer dummy __maybe_unused)
{
    unsigned keysym = event->keyval;
    unsigned state = event->state;
    // Here we process non-textual keys
    if (keysym == GDK_KEY_BackSpace)
        lineedit_backspace();
    else if (keysym == GDK_KEY_Tab)
        lineedit_autocomplete();
    else if ((state & GDK_CONTROL_MASK))
    {
        if ((keysym == GDK_KEY_A) || (keysym == GDK_KEY_a))
            lineedit_move_home();
        if (keysym == GDK_KEY_B || (keysym == GDK_KEY_b))
            lineedit_move_left();
        if (keysym == GDK_KEY_C || (keysym == GDK_KEY_c))
            lineedit_ctrl_c();
        if (keysym == GDK_KEY_D || (keysym == GDK_KEY_d))
            lineedit_delete_or_eof();
        if (keysym == GDK_KEY_E || (keysym == GDK_KEY_e))
            lineedit_move_end();
        if (keysym == GDK_KEY_F || (keysym == GDK_KEY_f))
            lineedit_move_right();
        if (keysym == GDK_KEY_G || (keysym == GDK_KEY_g))
            lineedit_backspace();
        if (keysym == GDK_KEY_H || (keysym == GDK_KEY_h))
            lineedit_delete_to_end();
        if (keysym == GDK_KEY_L || (keysym == GDK_KEY_l))
            lineedit_clear_screen();
        if (keysym == GDK_KEY_N || (keysym == GDK_KEY_n))
            lineedit_history_next();
        if (keysym == GDK_KEY_P || (keysym == GDK_KEY_p))
            lineedit_history_prev();
        if (keysym == GDK_KEY_T || (keysym == GDK_KEY_t))
            lineedit_swap_chars();
        if (keysym == GDK_KEY_Y || (keysym == GDK_KEY_y))
            lineedit_delete_line();
        if (keysym == GDK_KEY_W || (keysym == GDK_KEY_w))
            lineedit_delete_word_prev();
    }
    else if (keysym == GDK_KEY_Delete)
        lineedit_delete();
    else if (keysym == GDK_KEY_Down)
        lineedit_history_next();
    else if (keysym == GDK_KEY_End)
        lineedit_move_end();
    else if (keysym == GDK_KEY_Home)
        lineedit_move_home();
    else if ((keysym == GDK_KEY_Insert) || (keysym == GDK_KEY_KP_Insert))
        lineedit_toggle_insert_mode();
    else if (keysym == GDK_KEY_Left)
        lineedit_move_left();
    else if (keysym == GDK_KEY_Right)
        lineedit_move_right();
    else if (keysym == GDK_KEY_Up)
        lineedit_history_prev();
    else if (keysym == GDK_KEY_Tab)
        ;
    else if (keysym == GDK_KEY_Clear)
        ;
    else if (keysym == GDK_KEY_Pause)
        ;
    else if (keysym == GDK_KEY_Delete)
    {
    }
    else if (keysym == GDK_KEY_Return) {
        // End this lineedit session
        // Act on the string
        // Maybe add the string to the history
        lineedit_stop();
        console_move_to_column(0);
        console_move_down(1);

        char *script = lineedit_get_text();
        if (strlen(script) > 0) {
            // Call script callback with the current string
#if 1
            char *output = burro_app_window_eval_string_in_sandbox (script);
            if (output) {
                if (g_strcmp0 (output, "#<unspecified>") != 0)
                {
                    if (strncmp (output, "ERROR", 5) == 0)
                        console_set_fgcolor (CONSOLE_COLOR_RED);
                    console_write_utf8_string(output);
                    console_set_fgcolor (CONSOLE_COLOR_DEFAULT);
                    console_move_to_column(0);
                    console_move_down(1);
                }
                g_free(output);
            }
            
#endif

            //lineedit_start(linenoiseLineBuf, LINENOISE_MAX_LINE, L"->");
        }
        g_free (script);
        lineedit_start();
    }
    else if (keysym >= GDK_KEY_space && keysym <= GDK_KEY_ydiaeresis)
    {
        wchar_t input[2];
        input[0] = keysym;
        input[1] = L'\0';
        // if (autocomplete_flag)
        //     lineedit_autocomplete_text_input(input);
        // else
        lineedit_text_input(input);
    }
    return true;
}

gboolean
draw_event_terminal (GtkWidget *widget __maybe_unused,
                     cairo_t *cr,
                     gpointer data __maybe_unused)
{
    cairo_surface_t *surf;

    surf = console_render_to_cairo_surface ();
    cairo_surface_mark_dirty (surf);

    /* Now copy it to the screen */
    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_surface_destroy (surf);
    cairo_paint(cr);
    return TRUE;
}

gboolean
force_terminal_draw (gpointer user_data)
{
    GtkWidget *terminal = GTK_WIDGET(user_data);
    gtk_widget_queue_draw_area (terminal, 0, 0, 640, 312);
    return TRUE;
}


static void
burro_debug_window_init (BurroDebugWindow *win)
{
    gtk_widget_init_template (GTK_WIDGET (win));

    // Set up the terminal
    gtk_widget_set_events (GTK_WIDGET (win->terminal),
                           GDK_BUTTON_PRESS_MASK);
        
    win->key_press_event_signal_id =
        g_signal_connect (G_OBJECT (win),
                          "key-press-event",
                          G_CALLBACK (key_event_terminal),
                          win->terminal);
    win->terminal_draw_event_id =
        g_signal_connect (G_OBJECT(win->terminal), "draw",
                          G_CALLBACK (draw_event_terminal), NULL);
    lineedit_initialize();
    console_reset();
    console_enable_repl();
    win->terminal_timeout = 
        g_timeout_add (100, force_terminal_draw, win->terminal);
    
    // Hoop up the message view
    gtk_tree_view_set_model (win->message_view,
                             GTK_TREE_MODEL (burro_journal_get_store ()));
    {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes("Key", renderer,
                                                          "text", 0, NULL);
        gtk_tree_view_append_column (win->message_view, column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes("Value", renderer,
                                                          "text", 1, NULL);
        gtk_tree_view_append_column (win->message_view, column);
    }
    gtk_widget_show (GTK_WIDGET(win->message_view));

    // Set up REPL
    gtk_combo_box_set_active (GTK_COMBO_BOX (win->repl_combobox), 0);
    g_signal_connect (win->repl_combobox,
                      "changed",
                      G_CALLBACK(signal_action_repl_enabled),
                      win);

    // Hook up the VRAM viewer
    win->vram_list_store = vram_info_list_store_new ();
    gtk_tree_view_set_model (win->vram_tree_view, GTK_TREE_MODEL (win->vram_list_store));
    {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Bank",
                                                           renderer,
                                                           "text", VRAM_COLUMN_NAME,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->vram_tree_view), column);
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Type",
                                                           renderer,
                                                           "text", VRAM_COLUMN_TYPE,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->vram_tree_view), column);
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Filename",
                                                           renderer,
                                                           "text", VRAM_COLUMN_FILENAME,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->vram_tree_view), column);
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Size",
                                                           renderer,
                                                           "text", VRAM_COLUMN_SIZE,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->vram_tree_view), column);
    }
    gtk_widget_show (GTK_WIDGET(win->vram_tree_view));


    // Debugger
    g_signal_connect (G_OBJECT (win->pause_button),
                      "toggled",
                      G_CALLBACK (signal_debug_pause),
                      win);
    gtk_widget_set_sensitive (GTK_WIDGET (win->pause_button), TRUE);
    g_signal_connect (G_OBJECT (win->next_button),
                      "clicked",
                      G_CALLBACK (signal_debug_next),
                      win);
#if 0
    g_signal_connect (G_OBJECT (win->debug_refresh_button),
                      "clicked",
                      G_CALLBACK (signal_debug_refresh),
                      NULL);
#endif
    // Hook up the peek viewer
    win->debug_peek_list_store = debug_peek_list_store_new ();
    gtk_tree_view_set_model (win->debug_peek_tree_view, GTK_TREE_MODEL (win->debug_peek_list_store));
    {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Time",
                                                           renderer,
                                                           "text", DEBUG_COLUMN_TIME,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->debug_peek_tree_view), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Label",
                                                           renderer,
                                                           "text", DEBUG_COLUMN_NAME,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->debug_peek_tree_view), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Value",
                                                           renderer,
                                                           "text", DEBUG_COLUMN_VALUE,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->debug_peek_tree_view), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Stack",
                                                           renderer,
                                                           "text", DEBUG_COLUMN_STACK,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (win->debug_peek_tree_view), column);
    }
    gtk_widget_show (GTK_WIDGET(win->debug_peek_tree_view));

}

static gboolean
burro_debug_hide_on_delete (GtkWidget *widget,
                            GdkEventAny *event __maybe_unused)
{
    return gtk_widget_hide_on_delete (widget);
}

static void
burro_debug_window_dispose (GObject *object)
{
    BurroDebugWindow *win;
    win = BURRO_DEBUG_WINDOW (object);

    g_clear_object(&(win->vram_tree_view));
    G_OBJECT_CLASS (burro_debug_window_parent_class)->dispose (object);
}

static void
burro_debug_window_class_init (BurroDebugWindowClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(class);
    widget_class = GTK_WIDGET_CLASS (class);
    gobject_class->dispose = burro_debug_window_dispose;

    widget_class->delete_event = burro_debug_hide_on_delete;

    signals[STOP_REQUEST] =
        g_signal_new ("stop-requested",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[START_REQUEST] =
        g_signal_new ("start-requested",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[STEP_REQUEST] =
        g_signal_new ("step-requested",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[REPL_REQUEST] =
        g_signal_new ("repl-requested",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    gtk_widget_class_set_template_from_resource (widget_class, DEBUG_WINDOW);

#define BIND(x) \
    gtk_widget_class_bind_template_child (widget_class, BurroDebugWindow, x)

    BIND(terminal);
    BIND(message_view);
    BIND(message_scrolled_window);
    BIND(repl_combobox);
    BIND(vram_tree_view);
    BIND(update_rate_label);
    BIND(duty_cycle_label);
    BIND(pause_button);
    BIND(next_button);
    BIND(debug_peek_tree_view);

#undef BIND
}

BurroDebugWindow *
burro_debug_window_new (BurroAppWindow *parent __maybe_unused)
{
    debug_window_cur = g_object_new (BURRO_DEBUG_WINDOW_TYPE,  NULL);
    return debug_window_cur;
}

void
burro_debug_window_update_rate_label (BurroDebugWindow *win, const char *str)
{
    gtk_label_set_text (win->update_rate_label, str);
}

void
burro_debug_window_update_cycle_label (BurroDebugWindow *win, const char *str)
{
    gtk_label_set_text (win->duty_cycle_label, str);
}


SCM_DEFINE (G_burro_debug_win_console_write, "console-write-bytevector", 3, 0, 0,
            (SCM bv, SCM _index, SCM _count), "\
Takes the bytes from STORE between INDEX and INDEX + COUNT and writes\n\
them to the structured log.  It returns the number of bytes actually\n\
written.")
{
    size_t count = scm_to_size_t (_count);
    size_t index = scm_to_size_t (_index);
    if (count == 0)
        return scm_from_int(0);

    char *msg = g_malloc(count + 1);
    memcpy (msg, SCM_BYTEVECTOR_CONTENTS(bv) + index, count);
    msg[count] = '\0';
    g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE,
                      "MESSAGE", msg);
    free (msg);
    return _count;
}

#if 0
static void
console_write_icon (const gchar *c_icon_name)
{
    GError *error = NULL;
    GtkIconTheme *icon_theme;
    GdkPixbuf *pixbuf;

    if (!debug_window_cur)
        return;

    icon_theme = gtk_icon_theme_get_default ();
    pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                       c_icon_name, // icon name
                                       12, // icon size
                                       0,  // flags
                                       &error);
    if (!pixbuf)
    {
        g_warning ("Couldn’t load icon: %s", error->message);
        g_error_free (error);
    }

    // Use the pixbuf
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark (debug_window_cur->message_store,
                                      &iter,
                                      gtk_text_buffer_get_insert (debug_window_cur->message_store));

    gtk_text_buffer_insert_pixbuf(debug_window_cur->message_store,
                                  &iter,
                                  pixbuf);
    g_object_unref (pixbuf);
}

SCM_DEFINE (G_debug_console_write_icon,
            "console-write-icon", 1, 0, 0,
            (SCM name), "\
Writes the icon, given by its icon theme name, to the console.")
{
    char *c_icon_name = scm_to_utf8_string (name);
    console_write_icon (c_icon_name);
    free (c_icon_name);
    return SCM_BOOL_T;
}
#endif

SCM_DEFINE (G_debug_debug_peek_append, "debug-peek-append", 3, 0, 0,
            (SCM label, SCM value, SCM stack), "")
{
    gint64 now = g_get_monotonic_time();
    char *timestr = g_strdup_printf("%.3f", 1.0e-6 * now);
    char *slabel = scm_to_utf8_string(label);
    char *svalue = scm_to_utf8_string(value);
    char *sstack = scm_to_utf8_string(stack);
    if (!debug_window_cur)
    {
        g_info("debug-peek-append: %s: %s", slabel, svalue);
        free (timestr);
        free (slabel);
        free (svalue);
        free (sstack);
        return SCM_UNSPECIFIED;
    }

    GtkTreeIter iter;
    gtk_list_store_insert (debug_window_cur->debug_peek_list_store,
                           &iter,
                           0);

    gtk_list_store_set (debug_window_cur->debug_peek_list_store,
                        &iter,
                        DEBUG_COLUMN_TIME, timestr,
                        DEBUG_COLUMN_NAME, slabel,
                        DEBUG_COLUMN_VALUE, svalue,
                        DEBUG_COLUMN_STACK, sstack,
                        -1);
    g_free (timestr);
    g_free (slabel);
    g_free (svalue);
    g_free (sstack);
    return SCM_UNSPECIFIED;
}

void
burro_debug_window_init_guile_procedures ()
{
#ifndef SCM_MAGIC_SNARFER
#include "burro_debug_window.x"
#endif
    scm_c_export ( "console-write-bytevector",
                  "console-write-icon",
                  "debug-peek-append",
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
