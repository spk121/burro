#include <gtk/gtk.h>
#include <locale.h>
#include <libguile.h>
#include "burro_app.h"
#include "burro_journal.h"
#include "canvas.h"

int
main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");
    
    extern void scm_c_set_default_vm_engine_x (int x);
    scm_c_set_default_vm_engine_x (1);
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
