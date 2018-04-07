#ifndef BURRO_JOURNAL_H
#define BURRO_JOURNAL_H

#include <glib.h>
GLogWriterOutput burro_journal_writer (GLogLevelFlags log_level,
						 const GLogField *fields,
						 gsize n_fields,
						 gpointer user_data);
// void burro_journal_init ();
GtkTreeStore* burro_journal_get_store();

void burro_journal_init_guile_procedures ();

#endif
