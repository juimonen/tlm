/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
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

#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>

#include "common/tlm-log.h"
#include "common/tlm-utils.h"

static void launch_from_file(const char *file) {
  FILE *fp = NULL;
  char str[1024];
  GList *commands = NULL, *tmp_list = NULL;
  GList *argv_list = NULL;
  gchar **argv = NULL;
  gchar *command = NULL;

  if (!(fp = fopen(file, "r"))) {
    ERR("Failed to open file '%s':%s", file, strerror(errno));
    return;
  }
  
  while (fgets(str, sizeof(str) - 1, fp) != NULL) {
    gchar *cmd = g_strstrip(str);

    if (!strlen(cmd) || cmd[0] == '#') /* comment */
      continue;

    INFO("COMMAND: %s(%ld)\n", cmd, strlen(cmd));
    commands = g_list_append(commands, g_strdup(cmd));
  }

  fclose(fp);

  argv_list = tlm_utils_split_command_lines(commands);

  g_list_free_full(commands, g_free);

  if (g_list_length(argv_list) == 0) {
    WARN("No valid commands found in file '%s'!!!", file);
    return;
  }

  for (tmp_list = argv_list;
       tmp_list && tmp_list->next;
       tmp_list = tmp_list->next) {
    pid_t child_pid = 0;

    if (!(argv = (gchar **)tmp_list->data)) {
      continue;
    }

    if ((child_pid = fork()) < 0) {
      ERR("fork() failed: %s", strerror(errno));
    } else if (child_pid == 0) {
      /* child process */
      INFO("Launching command : %s, pid: %d, ppid: %d\n",
              argv[0], getpid(), getppid());
      execvp(argv[0], argv);
      WARN("exec failed: %s", strerror(errno));
      return;
    }

    g_strfreev(argv);
  }

  if (!tmp_list) {
    goto end;
  }

  argv = (gchar **)tmp_list->data;
  if (!argv) {
    goto end;
  }

  INFO("Launching command : %s, pid: %d, ppid: %d\n",
        argv[0], getpid(), getppid());
  g_list_free(argv_list);

  execvp(argv[0], argv);

  g_strfreev(argv);

  return;

end:
  g_list_free(argv_list);
}

static void help() {
  g_print("Usage:\n"
          "\ttlm-launcher -f script_file  - Launch commands from script_file.\n"
          "\t             -h              - Print this help message.\n");
}

int main(int argc, char *argv[]) {
  struct option opts[] = {
    { "file", required_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'h' },
    { 0, 0, NULL, 0 }
  };
  int i, c;
  char *file = NULL;

  pid_t child_pid = 0;

  tlm_log_init("tlm-launch");

  while ((c = getopt_long(argc, argv, "f:h", opts, &i)) != -1) {
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

  INFO("PID: %d\n", getpid());

  launch_from_file(file);

  return 0;
}
