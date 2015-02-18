/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2013-2014 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
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
#include <sys/types.h>
#include <glib.h>

#include "common/tlm-log.h"
#include "common/tlm-utils.h"

typedef struct {
  GPid pid;
  guint watcher;
} ChildInfo;

typedef struct {
  GMainLoop *loop;
  FILE *fp;
  guint socket_watcher;
  GHashTable *childs; /* { pid_t:ChildInfo* } */
} TlmLauncher;

static void _tlm_launcher_process (TlmLauncher *l);

static void
_child_info_free (gpointer data)
{
  ChildInfo *info = (ChildInfo *)data;
  if (info) {
    g_spawn_close_pid (info->pid);
    g_source_remove (info->watcher);
    g_slice_free (ChildInfo, info);
  }
}

static void
_tlm_launcher_init (TlmLauncher *l)
{
  if (!l) return;
  l->loop = g_main_loop_new (NULL, FALSE);
  l->fp = NULL;
  l->socket_watcher = 0;
  l->childs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                     NULL, _child_info_free);
}

static void
_tlm_launcher_deinit (TlmLauncher *l)
{
  if (!l) return;

  if (l->fp) {
    fclose (l->fp);
    l->fp = NULL;
  }

  g_hash_table_unref (l->childs);
  l->childs = 0;

  if (l->socket_watcher) {
    g_source_remove (l->socket_watcher);
    l->socket_watcher = 0;
  }
}

static void
_on_child_down_cb (GPid pid, gint status, gpointer userdata)
{
  TlmLauncher *l = (TlmLauncher *)userdata;

  DBG("Child dead: %d", pid);

  g_hash_table_remove (l->childs, GINT_TO_POINTER (pid));

  if (g_hash_table_size (l->childs) == 0) {
    DBG("All childs dead, going down...");
    kill (getpid(), SIGINT);
  }
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
      case 'L':
        argv = tlm_utils_split_command_line (cmd);
        if ((child_pid = fork()) < 0) {
          ERR("fork() failed: %s", strerror (errno));
        } else if (child_pid == 0) {
            /* child process */
            INFO("Launching command : %s, pid: %d, ppid: %d\n",
                argv[0], getpid (), getppid ());
            execvp(argv[0], argv);
            WARN("exec failed: %s", strerror (errno));
        } else if (control == 'M') {
          ChildInfo *info = g_slice_new0 (ChildInfo);
          info->pid = child_pid;
          info->watcher = g_child_watch_add (child_pid,
              (GChildWatchFunc)_on_child_down_cb, l);
          g_hash_table_insert (l->childs,
              GINT_TO_POINTER(child_pid), info);
        }
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
    { "help", no_argument, NULL, 'h' },
    { 0, 0, NULL, 0 }
  };
  int i, c;
  char *file = NULL;
  TlmLauncher launcher;

  tlm_log_init("tlm-launch");

  while ((c = getopt_long (argc, argv, "f:h", opts, &i)) != -1) {
    switch(c) {
      case 'h':
        help();
        return 0;
      case 'f':
        file = optarg;
    }
  }

  if (!file) {
    /* FIXME: Load from configuration ??? */
    help();
    return 0;
  }

  _tlm_launcher_init (&launcher);

  if (!(launcher.fp = fopen(file, "r"))) {
    ERR("Failed to open file '%s':%s", file, strerror(errno));
    _tlm_launcher_deinit (&launcher);
    return 0;
  }

  INFO("PID: %d\n", getpid());

  _tlm_launcher_process (&launcher);

  g_main_loop_run (launcher.loop);

  _tlm_launcher_deinit (&launcher);

  return 0;
}
