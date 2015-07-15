/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2013-2015 Intel Corporation.
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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <pwd.h>
#include <grp.h>
#include <sys/prctl.h>
#include <sys/capability.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "tlm-log.h"
#include "tlm-manager.h"
#include "tlm-seat.h"
#include "tlm-config.h"
#include "tlm-config-general.h"


static GMainLoop *main_loop = NULL;


static void
_on_manager_stopped_cb (TlmManager *manager, gpointer user_data)
{
    DBG ("manager stopped - quit mainloop");
    g_main_loop_quit (main_loop);
}

static gboolean
_on_sigterm_cb (gpointer data)
{
    DBG ("SIGTERM");

    TlmManager *manager = TLM_MANAGER(data);

    g_signal_connect (manager,
                      "manager-stopped",
                      G_CALLBACK (_on_manager_stopped_cb),
                      main_loop);
    tlm_manager_stop (manager);

    return FALSE;
}

static gboolean
_on_sighup_cb (gpointer data)
{
    DBG ("SIGHUP");

    TlmManager *manager = TLM_MANAGER(data);
    tlm_manager_sighup_received (manager);

    return FALSE;
}

static void
_setup_unix_signal_handlers (TlmManager *manager)
{
    if (signal (SIGINT, SIG_IGN) == SIG_ERR)
        WARN ("failed ignore SIGINT: %s", strerror(errno));

    g_unix_signal_add (SIGTERM, _on_sigterm_cb, (gpointer) manager);
    g_unix_signal_add (SIGHUP, _on_sighup_cb, (gpointer) manager);
}

int drop_privileges (const gchar* user)
{
    struct passwd *p;
    cap_t caps = NULL;
    uint64_t i, j = 0;
    /*
    uint64_t cap_flag = (1ULL << CAP_CHOWN) |
                        (1ULL << CAP_DAC_OVERRIDE) |
                        (1ULL << CAP_FOWNER) |
                        (1ULL << CAP_KILL) |
                        (1ULL << CAP_SETUID) |
                        (1ULL << CAP_SETPCAP) |
                        (1ULL << CAP_SYS_TTY_CONFIG) |
                        (1ULL << CAP_SETFCAP) |
                        (1ULL << CAP_SETGID);
    */
    unsigned long last_cap = (unsigned long)CAP_LAST_CAP;
    cap_value_t bits[last_cap + 1];

    p = getpwnam(user);
    if (!p) return -1;

    /* sets gids for the calling process */
    if (setresgid(p->pw_gid, p->pw_gid, p->pw_gid) < 0 ||
        setgroups(0, NULL) < 0) {
        DBG ("setresgid failed with error %s", strerror(errno));
        return -1;
    }

    /* sets uids for the calling process */
    if (prctl(PR_SET_KEEPCAPS, 1) < 0 ||
        setresuid(p->pw_uid, p->pw_uid, p->pw_uid) < 0 ||
        prctl(PR_SET_KEEPCAPS, 0) < 0) {
        DBG ("setresuid failed with error %s", strerror(errno));
        return -1;
    }

    for (i = 0; i <= last_cap; i++) {
        if (prctl(PR_CAPBSET_READ, i) > 0) {
        //if (cap_flag & (1ULL << i)) {
            bits[j++] = i;
            DBG ("cap %ld is included in bounding set and total is %ld",
                    (unsigned long)i, (unsigned long)j);
        }
    }
    if (!j) return 0;

    /* Set cap for effective flag as well, as systemd does not seem to set it
     * other than for permitted and inheritable flags*/
    caps = cap_init();
    if (!caps) return -1;

    if (cap_set_flag(caps, CAP_EFFECTIVE, j, bits, CAP_SET) < 0 ||
        cap_set_flag(caps, CAP_PERMITTED, j, bits, CAP_SET) < 0) {
        DBG ("cap_set_flag failed with error %s", strerror(errno));
        cap_free (caps);
        return -1;
    }

    if (cap_set_proc(caps) < 0) {
        DBG ("cap_set_proc failed with error %s", strerror(errno));
        cap_free (caps);
        return -1;
    }
    cap_free (caps);

    return 0;
}

int main(int argc, char *argv[])
{
    GError *error = 0;
    TlmManager *manager = 0;

    int ec;
    int notify_code = 0;
    int notify_fd[2];
    gboolean show_version = FALSE;
    gboolean fatal_warnings = FALSE;
    gboolean daemonize = FALSE;
    gchar *username = NULL;
    const gchar *tlm_user = "tlm";

    GOptionContext *opt_context = NULL;
    GOptionEntry opt_entries[] = {
        { "version", 'v', 0, 
          G_OPTION_ARG_NONE, &show_version, 
          "Login Manager Version", "Version" },
        { "fatal-warnings", 0, 0,
          G_OPTION_ARG_NONE, &fatal_warnings,
          "Make all warnings fatal", NULL },
        { "daemonize", 'D', 0,
          G_OPTION_ARG_NONE, &daemonize,
          "Launch the daemon in background", NULL},
        { "username", 'u', 0,
          G_OPTION_ARG_STRING, &username,
          "Username to use", NULL },
        {NULL }
    };
   
#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif

    tlm_log_init (G_LOG_DOMAIN);
    tlm_log_init ("TLM_COMMON");

    if(drop_privileges (tlm_user)) {
        ERR ("Unable to drop priviliges correctly");
        tlm_log_close (NULL);
        return -1;
    }

    opt_context = g_option_context_new ("");
    g_option_context_add_main_entries (opt_context, opt_entries, NULL);
    g_option_context_parse (opt_context, &argc, &argv, &error);
    g_option_context_free (opt_context);
    if (error) {
        ERR ("Error parsing options: %s", error->message);
        g_error_free (error);
        tlm_log_close (NULL);
        return -1;
    }

    if (show_version) {
        INFO("Version: "PACKAGE_VERSION"\n");
        tlm_log_close (NULL);
        return 0;
    }

    if (fatal_warnings) {
        GLogLevelFlags log_level = g_log_set_always_fatal (G_LOG_FATAL_MASK);
        log_level |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
        g_log_set_always_fatal (log_level);
    }

    if (daemonize) {
        if (pipe (notify_fd))
            ERR ("Failed to create launch notify pipe: %s", strerror (errno));
        if (fork ()) {
            close (notify_fd[1]);
            ec = read (notify_fd[0], &notify_code, sizeof (notify_code));
            close (notify_fd[0]);
            if (ec > 0)
                return notify_code;
            else
                return errno;
        } else {
            close (notify_fd[0]);
            if (setsid () == (pid_t) -1) {
                /* ignore error on purpose */
                DBG ("setsid FAILED");
            }
            if (!freopen ("/dev/null", "r", stdin))
                notify_code = errno;
            if (!freopen ("/dev/null", "a", stdout))
                notify_code = errno;
            if (!freopen ("/dev/null", "a", stderr))
                notify_code = errno;
            if (!notify_code)
                DBG ("freopen FAILED: %s", strerror (notify_code));
        }
    }

    main_loop = g_main_loop_new (NULL, FALSE);

    manager = tlm_manager_new (username);
    _setup_unix_signal_handlers (manager);
    tlm_manager_start (manager);

    if (daemonize) {
        ec = write (notify_fd[1], &notify_code, sizeof (notify_code));
        close (notify_fd[1]);
        if (ec <= 0)
            ERR ("Failed to notify parent process: %s", strerror (errno));
    }

    g_main_loop_run (main_loop);

    g_object_unref (G_OBJECT(manager));
    g_free (username);

    DBG ("clean shutdown");

    tlm_log_close (NULL);

    return 0;
}

