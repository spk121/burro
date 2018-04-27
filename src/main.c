
#include <gtk/gtk.h>
#include <locale.h>
#include <libguile.h>
#include "burro_app.h"
#include "burro_journal.h"
#include "canvas.h"

int
main (int argc, char *argv[])
{
    // This might quiet DBUS errors about Error retrieving
    // accessibility bus address
    g_setenv("NO_AT_BRIDGE", "1", FALSE);
    
    setlocale (LC_ALL, "");
    
    extern void scm_c_set_default_vm_engine_x (int x);
    scm_c_set_default_vm_engine_x (1);

#ifdef RELATIVE_PATHS
    // Set up a relative path to Guile's own scheme sources,
    // and our site directory sources.
    g_setenv("GUILE_LOAD_PATH", "share/guile/2.2:share/guile/site/2.2", TRUE);

    // Set up a relative path to Guile's compiled sources and
    // our compiled site sources.
    g_setenv("GUILE_LOAD_COMPILED_PATH",
             "lib/guile/2.2/ccache:lib/guile/2.2/site-ccache", TRUE);

    // Set up the path to where on-the-fly compilation gets saved.
    // ('guile/ccache/VERSION' gets appended to this directory)
    g_setenv("XDG_CACHE_HOME", "lib", TRUE);
#endif
    
    scm_init_guile ();

    g_log_set_writer_func (burro_journal_writer, NULL, NULL);
    
    return g_application_run (G_APPLICATION (burro_app_new ()), argc, argv);
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
