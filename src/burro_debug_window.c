#include "burro_debug_window.h"
#include "burro_canvas.h"
#include "burro_repl.h"
#include "burro_resources.h"

#include <gtk/gtk.h>
#include <libguile.h>

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
    GtkTextView *message_view;
    GtkTextBuffer *message_store;
    GtkScrolledWindow *message_scrolled_window;
    GtkEntry *console_entry;
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
};


static void
console_write_icon (const gchar *c_icon_name);

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


G_DEFINE_TYPE(BurroDebugWindow, burro_debug_window, GTK_TYPE_WINDOW);

static BurroDebugWindow *debug_window_cur;

static void
signal_action_console_activate (GtkEntry *entry, gpointer user_data)
{
    const char* txt = gtk_entry_get_text (entry);
    char *output = burro_app_window_eval_string_in_sandbox (txt);

    GtkTextIter iter;
    GtkTextBuffer *msgstore = debug_window_cur->message_store;
    gtk_text_buffer_get_end_iter (msgstore, &iter);
    gtk_text_buffer_place_cursor (msgstore, &iter);
    gtk_text_buffer_insert_at_cursor (msgstore, ">>\t", -1);
    gtk_text_buffer_insert_at_cursor (msgstore, txt, -1);
    gtk_text_buffer_insert_at_cursor (msgstore, "\n", -1);
    if (g_strcmp0 (output, "#<unspecified>") != 0)
    {
        if (strncmp (output, "ERROR:", 6) == 0)
        {
            console_write_icon ("dialog-error");
            gtk_text_buffer_insert_at_cursor (msgstore, output + strlen("ERROR:"), -1);
        }
        else
        {
            gtk_text_buffer_insert_at_cursor (msgstore, "<<\t", -1);
            gtk_text_buffer_insert_at_cursor (msgstore,
                                              output, -1);
        }
        gtk_text_buffer_insert_at_cursor (msgstore, "\n", -1);
    }
    gtk_text_buffer_get_end_iter (msgstore, &iter);
    gtk_text_buffer_place_cursor (msgstore, &iter);

#if 0
    gtk_text_view_scroll_to_iter (debug_window_cur->message_view,
                                  &iter,
                                  0.0, TRUE, 0.0, 1.0);
#endif
    GtkAdjustment *vadj
        = gtk_scrolled_window_get_vadjustment(debug_window_cur->message_scrolled_window);
    gtk_adjustment_set_value(vadj,
                             gtk_adjustment_get_upper(vadj)
                             - gtk_adjustment_get_page_size(vadj));
}

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
signal_debug_next (GtkButton *button, gpointer user_data)
{
    BurroDebugWindow *win = BURRO_DEBUG_WINDOW(user_data);
    gboolean active = gtk_toggle_button_get_active (win->pause_button);
    if (!active)
        g_signal_emit (win, signals[STEP_REQUEST], 0);

}

static void
timeout_action_destroy (gpointer data)
{

}

static void
log_func (const gchar *log_domain,
          GLogLevelFlags log_level,
          const gchar *message,
          gpointer user_data)
{
    GtkTextBuffer *msgstore = debug_window_cur->message_store;
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter (msgstore, &iter);
    gtk_text_buffer_place_cursor (msgstore, &iter);

    if (log_level == G_LOG_LEVEL_ERROR
        || log_level == G_LOG_LEVEL_CRITICAL)
        console_write_icon ("dialog-error");
    else if (log_level == G_LOG_LEVEL_WARNING)
        console_write_icon ("dialog-warning");
    else
        console_write_icon ("dialog-information");

    gtk_text_buffer_insert_at_cursor (msgstore, "\t", -1);
    gtk_text_buffer_insert_at_cursor (msgstore, message, -1);
    gtk_text_buffer_insert_at_cursor (msgstore, "\n", -1);
    gtk_text_buffer_get_end_iter (msgstore, &iter);

    gtk_text_view_scroll_to_iter (debug_window_cur->message_view,
                                  &iter,
                                  0.0, TRUE, 0.0, 1.0);
    GtkAdjustment *vadj
        = gtk_scrolled_window_get_vadjustment(debug_window_cur->message_scrolled_window);
    gtk_adjustment_set_value(vadj,
                             gtk_adjustment_get_upper(vadj)
                             - gtk_adjustment_get_page_size(vadj));
}

static void
burro_debug_window_init (BurroDebugWindow *win)
{
    GtkBuilder *builder;
    GMenuModel *menu;
    GAction *action;

    gtk_widget_init_template (GTK_WIDGET (win));

    // Construct the menu
#if 0
    g_action_map_add_action_entries (G_ACTION_MAP (win),
                                     win_entries, G_N_ELEMENTS (win_entries),
                                     win);
#endif
#if 0
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

#endif
    // Attach a text storage to the text box
    win->message_store = gtk_text_view_get_buffer (win->message_view);

    // Set up console entry
    g_signal_connect (win->console_entry,
                      "activate",
                      G_CALLBACK(signal_action_console_activate),
                      NULL);

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

// This callback is called when the application has asked this window
// to delete itself.
static  gboolean
burro_debug_window_delete (GtkWidget         *widget,
                         GdkEventAny         *event)
{
    gtk_widget_destroy(widget);
    return TRUE;
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

    widget_class->delete_event = gtk_widget_hide_on_delete;
#if 0
    widget_class->window_state_event = burro_debug_window_state;
#endif

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

    BIND(message_view);
    BIND(message_scrolled_window);
    BIND(repl_combobox);
    BIND(console_entry);
    BIND(vram_tree_view);
    BIND(update_rate_label);
    BIND(duty_cycle_label);
    BIND(pause_button);
    BIND(next_button);
    BIND(debug_peek_tree_view);

#undef BIND
}

BurroDebugWindow *
burro_debug_window_new (BurroAppWindow *parent)
{
    debug_window_cur = g_object_new (BURRO_DEBUG_WINDOW_TYPE,
                     NULL);
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

void
burro_debug_window_log_string (BurroDebugWindow *win,
                               GLogLevelFlags log_level,
                               const char *message)
{
    GtkTextBuffer *msgstore = win->message_store;
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter (msgstore, &iter);
    gtk_text_buffer_place_cursor (msgstore, &iter);

    if (log_level == G_LOG_LEVEL_ERROR
        || log_level == G_LOG_LEVEL_CRITICAL)
        console_write_icon ("dialog-error");
    else if (log_level == G_LOG_LEVEL_WARNING)
        console_write_icon ("dialog-warning");
    else
        console_write_icon ("dialog-information");

    gtk_text_buffer_insert_at_cursor (msgstore, "\t", -1);
    gtk_text_buffer_insert_at_cursor (msgstore, message, -1);
    gtk_text_buffer_insert_at_cursor (msgstore, "\n", -1);
    gtk_text_buffer_get_end_iter (msgstore, &iter);

    gtk_text_view_scroll_to_iter (win->message_view,
                                  &iter,
                                  0.0, TRUE, 0.0, 1.0);
    GtkAdjustment *vadj
        = gtk_scrolled_window_get_vadjustment(win->message_scrolled_window);
    gtk_adjustment_set_value(vadj,
                             gtk_adjustment_get_upper(vadj)
                             - gtk_adjustment_get_page_size(vadj));

}

SCM_DEFINE (G_burro_debug_win_console_write, "console-write-bytevector", 3, 0, 0,
            (SCM bv, SCM _index, SCM _count), "\
Takes the bytes from STORE between INDEX and INDEX + COUNT and writes\n\
them to Message window.  It returns the number of bytes actually\n\
written.")
{
    size_t count = scm_to_size_t (_count);
    size_t index = scm_to_size_t (_index);
    if (count == 0)
        return scm_from_int(0);

    if (!debug_window_cur)
    {
        fputs(SCM_BYTEVECTOR_CONTENTS(bv) + index, stderr);
        return _count;
    }

    GtkTextBuffer *msgstore = debug_window_cur->message_store;

    // The sandbox should never be allowed to fill up all the memory
    // with junk text.
    gint totsize = gtk_text_buffer_get_char_count (msgstore);
    if (totsize > 10*1024)
    {
        GtkTextIter start, halfway;
        gtk_text_buffer_get_iter_at_offset (msgstore, &start, 0);
        gtk_text_buffer_get_iter_at_offset (msgstore, &halfway, totsize / 2);
        gtk_text_buffer_delete (msgstore, &start, &halfway);
    }

    GtkTextIter iter;
    gtk_text_buffer_get_end_iter (msgstore, &iter);
    gtk_text_buffer_place_cursor (msgstore, &iter);
    gtk_text_buffer_insert_at_cursor (msgstore,
                                      SCM_BYTEVECTOR_CONTENTS(bv) + index,
                                      count);
    gtk_text_buffer_get_end_iter (msgstore, &iter);

    gtk_text_view_scroll_to_iter (debug_window_cur->message_view,
                                  &iter,
                                  0.0, TRUE, 0.0, 1.0);
    GtkAdjustment *vadj
        = gtk_scrolled_window_get_vadjustment(debug_window_cur->message_scrolled_window);
    gtk_adjustment_set_value(vadj,
                             gtk_adjustment_get_upper(vadj)
                             - gtk_adjustment_get_page_size(vadj));

    return _count;
}


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

SCM_DEFINE (G_debug_debug_peek_append, "debug-peek-append", 3, 0, 0,
            (SCM label, SCM value, SCM stack), "")
{
    gint64 now = g_get_monotonic_time();
    char *timestr = g_strdup_printf("%.3f", 1.0e-6 * now);
    char *slabel = scm_to_utf8_string(label);
    char *svalue = scm_to_utf8_string(value);
    char *sstack = scm_to_utf8_string(stack);
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
