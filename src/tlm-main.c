#include <glib.h>
#include <gio/gio.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "tlm-log.h"
#include "tlm-manager.h"


static void
_on_sigterm_cb (gpointer data)
{
    DBG("SIGTERM/SIGINT");
    g_main_loop_quit ((GMainLoop*)data);
}

static void
_on_sighup_cb (gpointer data)
{
    DBG("SIGHUP");
    /* FIXME: Do something, may be reload configuration  */
}

static void
_setup_unix_signal_handlers (GMainLoop *loop)
{
    g_unix_signal_add (SIGTERM, _on_sigterm_cb, loop);
    g_unix_signal_add (SIGINT, _on_sigterm_cb, loop);
    g_unix_signal_add (SIGHUP, _on_sighup_cb, loop);
}

int main(int argc, char *argv[])
{
    GError *error = 0;
    GMainLoop *main_loop = 0;
    TlmManager *manager = 0;

    gboolean show_version = FALSE;
    gboolean fatal_warnings = FALSE;

    GOptionContext *opt_context = NULL;
    GOptionEntry opt_entries[] = {
        { "version", 'v', 0, 
          G_OPTION_ARG_NONE, &show_version, 
          "Login Manager Version", "Version" },
        { "fatal-warnings", 0, 0,
          G_OPTION_ARG_NONE, &fatal_warnings,
          "Make all warnings fatal", NULL },
        {NULL }
    };
   
#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif

    opt_context = g_option_context_new ("Tizen Login Manager");
    g_option_context_add_main_entries (opt_context, opt_entries, NULL);
    g_option_context_parse (opt_context, &argc, &argv, &error);
    g_option_context_free (opt_context);
    if (error) {
        ERR ("Error parsing options: %s", error->message);
        g_error_free (error);
        return -1;
    }

    if (show_version) {
        INFO("Version: "PACKAGE_VERSION"\n");
        return 0;
    }

    if (fatal_warnings) {
        GLogLevelFlags log_level = g_log_set_always_fatal (G_LOG_FATAL_MASK);
        log_level |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
        g_log_set_always_fatal (log_level);
    }

    tlm_log_init ();

    main_loop = g_main_loop_new (NULL, FALSE);

    _setup_unix_signal_handlers (main_loop);

    manager = tlm_manager_new ();

    tlm_manager_start (manager);

    g_main_loop_run (main_loop);

    tlm_manager_stop (manager);
    g_object_unref (G_OBJECT(manager));

    DBG ("clean shutdown");

    tlm_log_close ();

    return 0;
}

