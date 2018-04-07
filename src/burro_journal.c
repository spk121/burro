#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libguile.h>

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

static const int log_level_val[8] = {
  G_LOG_LEVEL_ERROR,
  G_LOG_LEVEL_ERROR,
  G_LOG_LEVEL_ERROR,
  G_LOG_LEVEL_ERROR,
  G_LOG_LEVEL_WARNING,
  G_LOG_LEVEL_MESSAGE,
  G_LOG_LEVEL_INFO,
  G_LOG_LEVEL_DEBUG
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

static char safe_char (wchar_t wc);
static char *scm_to_variable_name_string (SCM s_str, size_t *len);

/* Given an alist of key/value pairs, this procedure
 * reformats the data to be accepted by the glib
 * structured logger.
 * The keys must be either strings or symbols.
 * The values must be either strings or bytevectors.
 * If the keys or values are the wrong type, they are ignored.

 * It will return the number of entries actually logged.
 */
SCM
burro_journal_send_alist_to_logger (SCM s_alist)
{
  int list_len = 0;
  int count = 0;
  int ret = -1;
  size_t c_key_len = 0;
  char *c_key = NULL;
  int priority = G_LOG_LEVEL_DEBUG;

  if (!scm_is_true (scm_list_p (s_alist))
      || scm_is_null (s_alist))
    return scm_from_int (0);
  
  list_len = scm_to_int (scm_length (s_alist));
  GLogField *c_entries
    = (GLogField *) malloc (sizeof (GLogField) * list_len);
  for (int i = 0; i < list_len; i ++)
    {
      SCM s_entry, s_key, s_val;
      s_entry = scm_list_ref (s_alist, scm_from_int (i));
      if (scm_is_true (scm_pair_p (s_entry)))
	{
	  s_key = scm_car (s_entry);
	  s_val = scm_cdr (s_entry);
	  if (scm_is_symbol (s_key))
	    s_key = scm_symbol_to_string (s_key);

	  if (scm_is_string (s_key)
	      && (scm_is_string (s_val)
		  || scm_is_bytevector (s_val)))
	    {
	      c_key = scm_to_variable_name_string (s_key, &c_key_len);
	      if (c_key == NULL)
		continue;
	      
	      if (scm_is_string (s_val))
		{
		  char *c_val = scm_to_utf8_string (s_val);
		  if (strlen (c_val) == 0)
		    {
		      free (c_key);
		      free (c_val);
		      continue;
		    }
		  c_entries[count].key = c_key;
		  c_entries[count].value = c_val;
		  c_entries[count].length = -1;

		  if (!strcmp (c_key, "PRIORITY"))
		    {
		      char *tailptr;
		      priority = strtol(c_val, &tailptr, 10);
		    }
		  
		  count ++;
		}
	      else if (scm_is_bytevector (s_val))
		{
		  if (SCM_BYTEVECTOR_LENGTH (s_val) == 0)
		    {
		      free (c_key);
		      continue;
		    }
		  size_t len = SCM_BYTEVECTOR_LENGTH (s_val);
		  char *c_val = malloc (len);
		  memcpy (c_val,
			  SCM_BYTEVECTOR_CONTENTS (s_val),
			  SCM_BYTEVECTOR_LENGTH (s_val));
		  c_entries[count].key = c_key;
		  c_entries[count].value = c_val;
		  c_entries[count].length = len;
		  count ++;
		}

	    }
	}
    }
  if (count > 0)
    {
      g_log_structured_array (log_level_val[priority], c_entries, count);
      /* for (int i = 0; i < count; i ++) */
      /* 	{ */
      /* 	  free (c_entries[i].key); */
      /* 	  free (c_entries[i].value); */
      /* 	} */
    }
  //  free (c_entries);
  //free (c_key);
  if (ret < 0)
    return scm_from_int (0);
  return scm_from_int (count);
}

/* Convert a wchar_t to an ASCII uppercase letter, digit, or
 * underscore. */
static char
safe_char (wchar_t wc)
{
  if (wc == (wchar_t) '_')
    return '_';
  else if (wc >= (wchar_t) '0' && wc <= (wchar_t) '9')
    return (char) wc;
  else if (wc >= (wchar_t) 'A' && wc <= (wchar_t) 'Z')
    return (char) wc;
  else if (wc >= (wchar_t) 'a' && wc <= (wchar_t) 'z')
    return (char) (wc - (wchar_t) ' ');
  else if (wc == 0xAA)
    return 'A';
  else if (wc == 0xB2)
    return '2';
  else if (wc == 0xB3)
    return '3';
  else if (wc == 0xB9)
    return '1';
  else if ((wc >= 0xC0 && wc <= 0xC6) || (wc >= 0xE0 && wc <= 0xE6))
    return 'A';
  else if (wc == 0xC7 || wc == 0xE7)
    return 'C';
  else if ((wc >= 0xC8 && wc <= 0xCB) || (wc >= 0xE8 && wc <= 0xEB))
    return 'E';
  else if ((wc >= 0xCC && wc <= 0xCF) || (wc >= 0xEC && wc <= 0xEF))
    return 'I';
  else if (wc == 0xD1 || wc == 0xF1)
    return 'N';
  else if ((wc >= 0xD2 && wc <= 0xD6) || wc == 0xD8
	   || (wc >= 0xF2 && wc <= 0xF6) || wc == 0xF8)
    return 'O';
  else if ((wc >= 0xD9 && wc <= 0xDC) || (wc >= 0xF9 && wc <= 0xFC))
    return 'U';
  else if (wc == 0xDF)
    return 'S';
  else if (wc == 0xFD || wc == 0xFF)
    return 'Y';
  else
    return '_';
}

/* This takes any string and tries to convert it into a string that is
 * safe for use as systemd journal variable name, e.g., with only
 * ASCII uppercase letters, digits, and underscores and starting with
 * a letter. */
static char *
scm_to_variable_name_string (SCM s_str, size_t *ret_len)
{
  SCM_ASSERT (scm_is_string (s_str), s_str, SCM_ARG1, "send-alist-to-journal");
  int underscore_start = 0;
  size_t len = scm_c_string_length (s_str);

  if (len == 0)
    return NULL;

  if (safe_char (SCM_CHAR (scm_c_string_ref (s_str, 0))) == '_')
    underscore_start = 1;
  
  char *c_str = (char *) malloc (sizeof(char) * (len + 1 + underscore_start));
  if (underscore_start)
    c_str[0] = 'X';
  for (int i = 0; i < len; i ++)
    {
      c_str[i + underscore_start]
	= safe_char (SCM_CHAR (scm_string_ref (s_str, scm_from_int (i))));
    }
  c_str[len + underscore_start] = '\0';
  *ret_len = len + underscore_start;
  return c_str;
}

void
burro_journal_init_guile_procedures ()
{
  scm_c_define_gsubr ("send-alist-to-logger", 1, 0, 0,
		      burro_journal_send_alist_to_logger);
  scm_c_export ("send-alist-to-logger", NULL);
}
