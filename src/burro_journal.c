#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

/* This is a store for message output and debugging functions.

   Messages can be generated in C with the g_log procedures from GLIB.
   Messages can be generated in Scheme from (mlg logging).

   In any case, all logging will be sent out the g_log_writer_default,
   which is the systemd journal if available, or to stdout if not
   available.

   Also, all log info will be held in memory here as a GtkTreeStore,
   so that if the developer view is enabled, the log info can be put
   in the tree view.

   Care must be made to constrain the length of the tree store in case
   of logging insanity. */

static GtkTreeStore *burro_journal_store = NULL;

static const char log_level_string[8][15] = {
  "EMERGENCY",
  "ALERT",
  "CRITICAL",
  "ERROR",
  "WARNING",
  "NOTICE",
  "INFO",
  "DEBUG"
};

GtkTreeStore *burro_journal_get_store()
{
  if (!burro_journal_store)
    {
      burro_journal_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

      GtkTreeIter iter;
      gtk_tree_store_prepend (burro_journal_store, &iter, NULL);
      
      gtk_tree_store_set (burro_journal_store,
			  &iter,
			  0, "INFO",
			  1, "Let's do this!",
			  -1);
    }
  
  return burro_journal_store;
}

GLogWriterOutput
burro_journal_writer (GLogLevelFlags log_level,
		      const GLogField *fields,
		      gsize n_fields,
		      gpointer user_data)
{
  GtkTreeIter iter;
  int i_message, i_priority;
  
  if (!burro_journal_store)
    burro_journal_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  for (i_message = 0; i_message < n_fields; i_message ++)
    {
      if (!strcmp ("MESSAGE", fields[i_message].key))
	break;
    }

  for (i_priority = 0; i_priority < n_fields; i_priority ++)
    {
      if (!strcmp ("PRIORITY", fields[i_priority].key))
	break;
    }

  gtk_tree_store_prepend (burro_journal_store, &iter, NULL);

  char *tailptr;
  long priority = strtol((char *)fields[i_priority].value, &tailptr, 10);
  
  gtk_tree_store_set (burro_journal_store,
		      &iter,
		      0, log_level_string[priority],
		      1, fields[i_message].value,
		      -1);
  for (int i = 0; i < n_fields; i ++)
    {
      if (i != i_message && i != i_priority)
	{
	  gboolean valid;
	  valid = g_utf8_validate (fields[i].value,
				   fields[i].length,
				   NULL);
	  if (valid)
	    {
	      GtkTreeIter iter2;
	      gtk_tree_store_append (burro_journal_store, &iter2, &iter);
	      gtk_tree_store_set (burro_journal_store, &iter2,
				  0, fields[i].key,
				  1, fields[i].value,
				  -1);
	    }
	}
    }
  
  // Also, log this by the normal method, too.
  g_log_writer_default (log_level, fields, n_fields, user_data);
  return G_LOG_WRITER_HANDLED;
}
