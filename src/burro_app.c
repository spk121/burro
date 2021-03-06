#include "../config.h"
#include "burro_app.h"
#include "burro_app_win.h"
#include "burro_preferences_dialog.h"
#include "burro_resources.h"

#include <gtk/gtk.h>
#include <libguile.h>

#ifdef _WIN32 
#define __maybe_unused
#else
#define __maybe_unused __attribute__((unused))
#endif

struct _BurroApp
{
    GtkApplication parent;
};

G_DEFINE_TYPE(BurroApp, burro_app, GTK_TYPE_APPLICATION);

static void
action_about (GSimpleAction *action __maybe_unused,
              GVariant *parameter __maybe_unused,
              gpointer user_data)
{
    BurroApp *app;
    GtkWindow *window;

    app = BURRO_APP (user_data);
    window = gtk_application_get_active_window (GTK_APPLICATION (app));

    g_return_if_fail (window);

    static const char *authors[] = {
        "Michael Gran <spk121@yahoo.com>",
        NULL
    };

    gtk_show_about_dialog (GTK_WINDOW (window),
                           "program-name", "Burro Engine",
                           "version", PACKAGE_VERSION,
                           "copyright", "Copyright 2014-2018 Michael Gran",
                           "authors", authors,
                           "website", "https://github.com/spk121/burro",
                           "logo-icon-name", "burro-engine",
                           "license-type", GTK_LICENSE_GPL_3_0,
                           NULL);
}

static void *
app_quit_guard (void * app)
{
    g_usleep(1);
    g_application_quit (G_APPLICATION (app));
    return NULL;
}

static void
action_quit (GSimpleAction *action __maybe_unused,
             GVariant *parameter __maybe_unused,
             gpointer app)
{
    GList *windows;
    scm_gc();
    g_usleep(1000);

loop: 
    windows = gtk_application_get_windows (GTK_APPLICATION (app));
    if (windows && windows->data)
    {
        gtk_widget_destroy (windows->data);
        goto loop;
    }
    scm_without_guile (app_quit_guard, app);
    scm_gc();
    g_usleep(1000);
}

static GActionEntry app_entries[] =
{
    {"about", action_about, NULL, NULL, NULL, {0, 0, 0}},
    {"quit", action_quit, NULL, NULL, NULL, {0, 0, 0}},
};


static void
burro_app_startup (GApplication *gapp)
{
    GApplicationClass *gclass = G_APPLICATION_CLASS (burro_app_parent_class);
    BurroApp *app = BURRO_APP (gapp);
    GtkApplication *gtkapp = GTK_APPLICATION(gapp);
    GtkBuilder *builder;
    GMenuModel *app_menu;

    gclass->startup (gapp);

    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     app_entries, G_N_ELEMENTS (app_entries),
                                     app);

    builder = gtk_builder_new_from_resource (APP_MENU);

    app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
    gtk_application_set_app_menu (gtkapp, app_menu);
    g_object_unref (builder);

    /* Set GNOME properties that PulseAudio likes to have */
    g_set_application_name("Burro Engine");
    g_setenv("PULSE_PROP_media.role", "game", TRUE);
    gtk_window_set_default_icon_name("applications-games");
}

static void
burro_app_activate (GApplication *app)
{
    BurroAppWindow *win;

    win = burro_app_window_new (BURRO_APP (app));
    GFile *f = g_file_new_for_path("share/burro-engine/game.burro");
    burro_app_window_open (win, f);
    gtk_window_present (GTK_WINDOW (win));
}

static void
burro_app_open (GApplication *app,
                GFile **files,
                gint n_files,
                const gchar *hint __maybe_unused)
{
    BurroAppWindow *win;

    if (n_files != 1)
    {
        // We can only handle one file.
        fprintf (stderr, "Too many command-line arguments\n");
        return;
    }

    win = burro_app_window_new (BURRO_APP (app));
    burro_app_window_open (win, files[0]);
    gtk_window_present (GTK_WINDOW (win));
}

static void
burro_app_init (BurroApp *app __maybe_unused)
{
}

////////////////////////////////////////////////////////////////
// CLASS INITIALIZATION

static void
burro_app_class_init (BurroAppClass *class)
{
    GApplicationClass *gclass = G_APPLICATION_CLASS(class);

    /* GObject Signals */
    gclass->startup = burro_app_startup;
    gclass->activate = burro_app_activate;
    gclass->open = burro_app_open;
}

////////////////////////////////////////////////////////////////
// PUBLIC C API

BurroApp *
burro_app_new (void)
{
    return g_object_new (BURRO_APP_TYPE,
                         "application-id", DOMAIN,
                         "flags", (G_APPLICATION_NON_UNIQUE
                                   | G_APPLICATION_HANDLES_OPEN),
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
