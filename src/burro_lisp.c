// #include "x.h"
#include "burro_paths.h"
#include "burro_lisp.h"
#include "burro_app_win.h"
#include "burro_journal.h"
#include "canvas.h"
#include "canvas_vram.h"

/* In this module a couple of things happen.

   1. We create a "burro engine" module, which has all of the
   C-defined engine-specific functions, including idle callbacks,
   mouse click callbacks, backdrop, background, spritesheet,
   textbox, and audio functions.

   2. We load the "burro" module into the (guile-user) environment,
   along with other necessary modules to allow a REPL to be 
   kicked off.  The "burro" module is scheme, but, it
   uses "burro engine".

   3. When we try to open a game file, we make a anonymous sandbox
   environment, which descends from the limited "ice-9 sandbox".
   This anonymous module has the all-pure-and-impure-bindings set,
   plus some of the procedures from the "burro" module.  The
   game file is eval'd in that sandbox.

   Remember to detach all "burro" hook functions before loading a
   new sandbox.
*/

#define __maybe_unused __attribute__((unused))

static void add_site_dir_to_load_path();
static SCM add_to_load_path (void *path);
static void create_burro_engine_module();
static SCM scm_init_burro_engine_module (void *data);
static void init_burro_engine (void *unused);


SCM burro_lisp_new ()
{
    SCM burro_user_module = scm_c_define_module ("guile-user", NULL, NULL);
    scm_set_current_module (burro_user_module);

    add_site_dir_to_load_path();

    // scm_c_use_module ("ice-9 readline");
    scm_c_use_module ("ice-9 eval-string");
    scm_c_use_module ("ice-9 sandbox");
    scm_c_use_module ("srfi srfi-1");
    scm_c_use_module ("rnrs bytevectors");
    scm_c_use_module ("system repl repl");
    scm_c_use_module ("system repl server");
    scm_c_use_module ("system repl coop-server");
    scm_c_use_module ("system vm trap-state");

    create_burro_engine_module();
    
    return burro_user_module;
}

static void
add_site_dir_to_load_path()
{
    char *guilesitedir = NULL;
    SCM ret;
    
    /* Add Burro's data directory to the load path. */
#ifdef GUILESITEDIR
    guilesitedir = g_locale_to_utf8 (GUILESITEDIR, -1, NULL, NULL, NULL);
    if (!guilesitedir)
        g_critical ("Could not add the Guile site directory to the load path");
    else
    {
        ret = scm_c_catch (SCM_BOOL_T,
                           add_to_load_path, guilesitedir,
                           default_error_handler,
                           NULL,
                           NULL, NULL);
        if (scm_is_false (ret))
        {
            g_critical ("Could not add '%s' to Guile load path", guilesitedir);
        }
        free (guilesitedir);
    }
#endif
}

static SCM
add_to_load_path (void *path)
{
    char *cmd = g_strdup_printf("(add-to-load-path \"%s\")\n", (char *)path);
    SCM val = scm_c_eval_string (cmd);
    free (cmd);
    return val;
}

static SCM
use_module (void *x)
{
    scm_c_use_module ((char *) x);
    return SCM_UNSPECIFIED;
}

static void
create_burro_engine_module()
{
    // Loading the internal Burro functions.
    SCM burro_engine =  scm_c_catch (SCM_BOOL_T,
                                     scm_init_burro_engine_module, NULL,
                                     default_error_handler,
                                     NULL,
                                     NULL, NULL);
    if (scm_is_true (burro_engine))
        scm_c_catch (SCM_BOOL_T,
                     use_module, (void *)"burro",
                     default_error_handler,
                     NULL,
                     NULL, NULL);        
        
}

static SCM
scm_init_burro_engine_module (void *data)
{
    return scm_c_define_module ("burro engine", init_burro_engine, data);
}

static void
init_burro_engine (void *unused __maybe_unused)
{
    // Load up the C-defined procedures
    burro_app_win_init_guile_procedures();
    burro_debug_window_init_guile_procedures();
    burro_journal_init_guile_procedures();
    canvas_init_guile_procedures();
}

SCM
default_error_handler (void *data __maybe_unused,
                       SCM key, SCM vals)
{
    /* if (data == NULL) */
    /*     return SCM_BOOL_F; */
  
    /* char **err_string = (char **) data; */
    char *c_key;
    SCM subr, message, args, rest;
    SCM message_args, formatted_message;

    /* Key is the exception type, a symbol. */
    /* exception is a list of 4 elements:
       - subr: a subroutine name (symbol?) or #f
       - message: a format string
       - args: a list of arguments that are tokens for the message
       - rest: the errno, if any */
    if (scm_is_true (key))
        c_key = scm_to_locale_string (scm_symbol_to_string (key));
    else
        c_key = NULL;

    if (c_key && ((!strcmp (c_key, "unbound-variable"))
                  || (!strcmp (c_key, "wrong-type-arg"))
                  || (!strcmp (c_key, "misc-error")))) {
    
        subr = scm_list_ref (vals, scm_from_int (0));
        message = scm_list_ref (vals, scm_from_int (1));
        args = scm_list_ref (vals, scm_from_int (2));
        rest = scm_list_ref (vals, scm_from_int (3));

        message_args = scm_simple_format (SCM_BOOL_F, message, args);

        if (scm_is_true (subr))
            formatted_message = scm_simple_format (SCM_BOOL_F,
                                                   scm_from_locale_string ("Error ~S: ~A~%"),
                                                   scm_list_2 (subr, message_args));
        else
            formatted_message = scm_simple_format (SCM_BOOL_F,
                                                   scm_from_locale_string ("Error: ~A~%"),
                                                   scm_list_1 (message_args));
    }
    else
        // This is some key for which I don't know the format for the arguments,
        // so I'll just print it out raw.
        formatted_message = scm_simple_format (SCM_BOOL_F,
                                               scm_from_locale_string ("Error ~S: ~S~%"),
                                               scm_list_2 (key, vals));

    char *msg = scm_to_utf8_string (formatted_message);
    g_log_structured (G_LOG_DOMAIN,
                      G_LOG_LEVEL_ERROR,
                      "KEY", c_key ? c_key : "NONE",
                      "MESSAGE", msg);
    free (msg);
    free (c_key);

    return SCM_BOOL_F;
}

static SCM
public_ref_burro_make_sandbox (void *unused __maybe_unused)
{
    return scm_c_public_ref("burro", "make-sandbox");
}

static SCM
public_ref_burro_load_file_into_sandbox (void *unused __maybe_unused)
{
    return scm_c_public_ref("burro", "load-file-into-sandbox");
}

static SCM
make_sandbox_with_path (void *c_path)
{
    SCM sandbox_func = public_ref_burro_load_file_into_sandbox (NULL);
    SCM s_path = scm_from_locale_string (c_path);
    return scm_call_1 (sandbox_func, s_path);
}

static SCM
make_sandbox (void *unused __maybe_unused)
{
    SCM sandbox_func = public_ref_burro_make_sandbox (NULL);
    return scm_call_0(sandbox_func);
}

SCM
burro_make_sandbox (GFile *file, char **err_string)
{
    // Try to parse the file, if any.
    SCM sandbox;
    if (file)
    {
        char *c_path = g_file_get_path (file);
        sandbox =  scm_c_catch (SCM_BOOL_T,
                                     make_sandbox_with_path, c_path,
                                     default_error_handler,
                                     NULL,
                                     NULL, NULL);
        g_free (c_path);
    }
    else
    {
        sandbox =  scm_c_catch (SCM_BOOL_T,
                                make_sandbox, NULL,
                                default_error_handler,
                                NULL,
                                NULL, NULL);
    }

    if (scm_is_string (sandbox))
    {
        // The loading failed.  The returned string says why.
        *err_string = scm_to_locale_string (sandbox);
        return SCM_BOOL_F;
    }

    return sandbox;
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
