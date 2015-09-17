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

#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <glib.h>

#include "common/tlm-log.h"
#include "common/tlm-utils.h"
#include "tlm-process-manager.h"

typedef struct {
  GMainLoop *loop;
  guint sig_source_id[2];
  FILE *fp;
  guint socket_watcher;
  TlmProcessManager *proc_manager;
} TlmLauncher;

static void _tlm_launcher_process (TlmLauncher *l);

static gboolean
_handle_quit_signal (gpointer user_data)
{
    TlmLauncher *l = (TlmLauncher *) user_data;

    g_return_val_if_fail (l != NULL, G_SOURCE_CONTINUE);
    DBG ("Received quit signal");
    if (l->proc_manager) {
        g_object_unref (l->proc_manager);
        l->proc_manager = NULL;
    }
    if (l->loop)
        g_main_loop_quit (l->loop);

    return G_SOURCE_CONTINUE;
}

static void
_install_sighandlers (TlmLauncher *l)
{
    if (signal (SIGINT, SIG_IGN) == SIG_ERR)
        WARN ("failed to ignore SIGINT: %s", strerror(errno));

    l->sig_source_id[0] = g_unix_signal_add (SIGTERM, _handle_quit_signal, l);
    l->sig_source_id[1] = g_unix_signal_add (SIGHUP, _handle_quit_signal, l);

    if (prctl(PR_SET_PDEATHSIG, SIGHUP))
        WARN ("failed to set parent death signal");
}

static void
_tlm_launcher_init (TlmLauncher *l)
{
  if (!l) return;
  l->loop = g_main_loop_new (NULL, FALSE);
  l->fp = NULL;
  l->socket_watcher = 0;
  l->proc_manager = 0;
  _install_sighandlers (l);
}

static void
_tlm_launcher_deinit (TlmLauncher *l)
{
  if (!l) return;

  if (l->fp) {
      fclose (l->fp);
      l->fp = NULL;
  }

  if (l->socket_watcher) {
      g_source_remove (l->socket_watcher);
      l->socket_watcher = 0;
  }
  if (l->proc_manager)
      g_object_unref (l->proc_manager);

  g_source_remove (l->sig_source_id[1]);
  g_source_remove (l->sig_source_id[0]);
}

static gboolean
_continue_launch (gpointer userdata)
{
  _tlm_launcher_process ((TlmLauncher*)userdata);
  return FALSE;
}


static void
_on_socket_ready (
    const gchar *socket,
    gboolean is_final,
    GError *error,
    gpointer userdata)
{
  DBG("Socket Ready; %s", socket);
  if (is_final) {
    ((TlmLauncher *)userdata)->socket_watcher = 0;
    g_idle_add (_continue_launch, userdata);
  }
}

/*
 * file syntax;
 * M: command -> fork & exec and monitor child
 * W: socket/file -> Wait for socket ready before moving forward
 * L: command -> Launch process
 */

static void _tlm_launcher_process (TlmLauncher *l)
{
  char str[1024];
  gchar **argv = NULL;
  gint wait = 0;
  pid_t child_pid = 0;

  if (!l || !l->fp) return;

  while (fgets(str, sizeof(str) - 1, l->fp) != NULL) {
    char control = 0;
    gchar *cmd = g_strstrip(str);

    if (!strlen(cmd) || cmd[0] == '#') /* comment */
      continue;

    INFO("Processing %s\n", cmd);
    control = cmd[0];
    cmd = g_strstrip (cmd + 2);
    switch (control) {
      case 'M':
        tlm_process_manager_launch_process (l->proc_manager, cmd, FALSE, NULL,
                NULL);
        break;
      case 'L':
        tlm_process_manager_launch_process (l->proc_manager, cmd, TRUE, NULL,
                NULL);
        break;
      case 'W': {
        gchar **sockets = g_strsplit(cmd, ",", -1);
        l->socket_watcher = tlm_utils_watch_for_files (
            (const gchar **)sockets, _on_socket_ready, l);
        g_strfreev (sockets);
        if (l->socket_watcher) return;
        }
        break;
      default:
        WARN("Ignoring unknown control '%c' for command '%s'", control, cmd);
    }
  }

  fclose (l->fp);
  l->fp = NULL;
}

static void help ()
{
  g_print("Usage:\n"
          "\ttlm-launcher -f script_file  - Launch commands from script_file.\n"
          "\t             -h              - Print this help message.\n");
}

int main (int argc, char *argv[])
{
  struct option opts[] = {
    { "file", required_argument, NULL, 'f' },
    { "sessionid", required_argument, NULL, 's' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, NULL, 0 }
  };
  int i, c;
  gchar *file = NULL;
  TlmLauncher launcher;
  gchar *address = NULL;
  TlmConfig *config = NULL;
  TlmProcessManager *proc_manager = NULL;
  const gchar *runtime_dir = NULL;
  gchar *sessionid = NULL;

  tlm_log_init ("TLM_LAUNCHER");
  tlm_log_init ("TLM_COMMON");

  while ((c = getopt_long (argc, argv, "f:s:h", opts, &i)) != -1) {
    switch(c) {
      case 'h':
        help();
        return 0;
      case 'f':
        file = g_strdup (optarg);
        DBG("file found %s", file);
        break;
      case 's':
        sessionid = g_strdup (optarg);
        DBG("sessionid found %s", sessionid);
        break;
    }
  }

  if (!file) {
    /* FIXME: Load from configuration ??? */
    help();
    g_free (sessionid);
    return 0;
  }

  _tlm_launcher_init (&launcher);

  if (!(launcher.fp = fopen(file, "r"))) {
    WARN("Failed to open file '%s':%s", file, strerror(errno));
    _tlm_launcher_deinit (&launcher);
    g_free (file);
    g_free (sessionid);
    return 0;
  }
  g_free (file);

  runtime_dir = g_getenv ("XDG_RUNTIME_DIR");

  if (sessionid && runtime_dir)
	  address = g_strdup_printf ("unix:path=%s/%s", runtime_dir,
              sessionid);
  else if (sessionid)
      address = g_strdup_printf ("unix:path=/run/user/%d/%s", getuid(),
              sessionid);
  else if (runtime_dir)
      address = g_strdup_printf ("unix:path=%s/%d", runtime_dir, getpid());
  else
      address = g_strdup_printf ("unix:path=/run/user/%d/%d", getuid(),
              getpid());

  config = tlm_config_new ();
  launcher.proc_manager = tlm_process_manager_new (config, address, getuid());
  DBG ("Tlm launcher pid:%d, dbus addr: %s, sessionid: %s, runtimedir: %s\n",
          getpid(), address, sessionid, runtime_dir);
  g_free (sessionid);
  g_free (address); address = NULL;

  _tlm_launcher_process (&launcher);

  g_main_loop_run (launcher.loop);

  g_object_unref (config);
  _tlm_launcher_deinit (&launcher);

  tlm_log_close (NULL);
  return 0;
}
