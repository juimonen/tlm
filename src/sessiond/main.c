/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <config.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <glib-unix.h>
#include <glib.h>
#include <gio/gio.h>
#include <sys/prctl.h>

#include "common/tlm-log.h"
#include "tlm-session-daemon.h"

static TlmSessionDaemon *_daemon = NULL;
static guint _sig_source_id[3];

static void
_on_daemon_closed (gpointer data, GObject *server)
{
    _daemon = NULL;
    DBG ("Daemon closed");
    if (data) g_main_loop_quit ((GMainLoop *)data);
}

static gboolean
_handle_quit_signal (gpointer user_data)
{
    GMainLoop *ml = (GMainLoop *) user_data;

    g_return_val_if_fail (ml != NULL, FALSE);
    DBG ("Received quit signal");
    if (ml) g_main_loop_quit (ml);

    return FALSE;
}

static void
_install_sighandlers (GMainLoop *main_loop)
{
    GSource *source = NULL;
    GMainContext *ctx = g_main_loop_get_context (main_loop);

    source = g_unix_signal_source_new (SIGTERM);
    g_source_set_callback (source,
                           _handle_quit_signal,
                           main_loop,
                           NULL);
    _sig_source_id[0] = g_source_attach (source, ctx);

    source = g_unix_signal_source_new (SIGINT);
    g_source_set_callback (source,
                           _handle_quit_signal,
                           main_loop,
                           NULL);
    _sig_source_id[1] = g_source_attach (source, ctx);

    source = g_unix_signal_source_new (SIGHUP);
    g_source_set_callback (source,
                           _handle_quit_signal,
                           main_loop,
                           NULL);
    _sig_source_id[2] = g_source_attach (source, ctx);

    if (prctl(PR_SET_PDEATHSIG, SIGHUP))
        WARN ("failed to set parent death signal");
}

int main (int argc, char **argv)
{
    GMainLoop *main_loop = NULL;
    gint in_fd = 0, out_fd = 1;

    /* Duplicates stdin and stdout descriptors and point the descriptors
     * to /dev/null to avoid anyone writing to descriptors
     * */
    in_fd = dup(0);
    if (in_fd == -1) {
        WARN ("Failed to dup stdin : %s(%d)", strerror(errno), errno);
        in_fd = 0;
    }
    if (!freopen("/dev/null", "r+", stdin)) {
        WARN ("Unable to redirect stdin to /dev/null");
    }

    out_fd = dup(1);
    if (out_fd == -1) {
        WARN ("Failed to dup stdout : %s(%d)", strerror(errno), errno);
        out_fd = 1;
    }

    if (!freopen("/dev/null", "w+", stdout)) {
        WARN ("Unable to redirect stdout to /dev/null");
    }

    /* Reattach stdout to stderr */
    //dup2 (2, 1);

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    DBG ("old pgid=%u", getpgrp ());

    _daemon = tlm_session_daemon_new (in_fd, out_fd);
    if (_daemon == NULL) {
        return -1;
    }

    main_loop = g_main_loop_new (NULL, FALSE);
    g_object_weak_ref (G_OBJECT (_daemon), _on_daemon_closed, main_loop);
    _install_sighandlers (main_loop);

    tlm_log_init(G_LOG_DOMAIN);

    DBG ("Entering main event loop");

    g_main_loop_run (main_loop);

    if(_daemon) {
        g_object_unref (_daemon);
    }

    if (main_loop) {
        g_main_loop_unref (main_loop);
    }
    tlm_log_close (NULL);
    return 0;
}
