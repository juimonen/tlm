/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <glib.h>
#include <gio/gio.h>
#include <glib-unix.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "tlm-log.h"
#include "tlm-manager.h"
#include "tlm-seat.h"


static gboolean
_on_sigterm_cb (gpointer data)
{
    DBG("SIGTERM/SIGINT");
    g_main_loop_quit ((GMainLoop*)data);

    return FALSE;
}

static gboolean
_on_sighup_cb (gpointer data)
{
    DBG("SIGHUP");
    /* FIXME: Do something, may be reload configuration  */
    return FALSE;
}

static void
_setup_unix_signal_handlers (GMainLoop *loop)
{
    g_unix_signal_add (SIGTERM, _on_sigterm_cb, (gpointer)loop);
    g_unix_signal_add (SIGINT, _on_sigterm_cb, (gpointer)loop);
    g_unix_signal_add (SIGHUP, _on_sighup_cb, (gpointer)loop);
}

static void
_on_seat_added (TlmManager *manager, TlmSeat *seat, gpointer data)
{
    const gchar *uname = (const gchar *)data;
    DBG("%s Seat Added", tlm_seat_get_id (seat));
    DBG ("starting auth session for user %s", uname);
    if (!tlm_seat_creat_session (seat, "tlm-guestlogin", uname)) {
        WARN ("Failed to start session");
        exit (0);
    }
}

int main(int argc, char *argv[])
{
    GError *error = 0;
    GMainLoop *main_loop = 0;
    TlmManager *manager = 0;

    gboolean show_version = FALSE;
    gboolean fatal_warnings = FALSE;
    gchar *username;

    GOptionContext *opt_context = NULL;
    GOptionEntry opt_entries[] = {
        { "version", 'v', 0, 
          G_OPTION_ARG_NONE, &show_version, 
          "Login Manager Version", "Version" },
        { "fatal-warnings", 0, 0,
          G_OPTION_ARG_NONE, &fatal_warnings,
          "Make all warnings fatal", NULL },
        { "username", 'u', 0,
          G_OPTION_ARG_STRING, &username,
          "Username to use", NULL },
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

    g_signal_connect(manager, "seat-added", 
            G_CALLBACK(_on_seat_added), username);

    tlm_manager_start (manager);

    g_main_loop_run (main_loop);

    tlm_manager_stop (manager);
    g_object_unref (G_OBJECT(manager));

    DBG ("clean shutdown");

    tlm_log_close ();

    return 0;
}

