/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2015 Intel Corporation.
 *
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include "common/tlm-config-general.h"
#include "common/tlm-config-seat.h"

int main (int argc, char *argv[])
{
    int retval = -1;
    int lockfd;
    int autologin = -1;
    int pause = -1;
    int setupterm = -1;
    int active = -1;
    int nwatch = -1;
    int vtnr = -1;
    int rtimedir = -1;
    gint nseats;
    gint seatx;
    gchar *cf = NULL;
    gchar *sname = NULL;
    gchar *username = NULL;
    gchar *session_type = NULL;
    gchar *pam_defserv = NULL;
    gchar *pam_serv = NULL;
    gchar *watchx = NULL;
    gchar *rtimemode = NULL;
    GKeyFile *kf;
    GError *error = NULL;
    GOptionContext *opts;
    GOptionEntry main_entries[] = {
        { "config-file", 'c',
          0, G_OPTION_ARG_FILENAME, &cf,
          "configuration file name",
          NULL },
        { "seat", 's',
          0, G_OPTION_ARG_STRING, &sname,
          "seat id",
          NULL },
        { "username", 'u',
          0, G_OPTION_ARG_STRING, &username,
          "default user name",
          NULL },
        { "autologin", 'a',
          0, G_OPTION_ARG_INT, &autologin,
          "autologin enable/disable",
          NULL },
        { "pause", 'p',
          0, G_OPTION_ARG_INT, &pause,
          "pause session enable/disable",
          NULL },
        { "setupterm", 't',
          0, G_OPTION_ARG_INT, &setupterm,
          "setup terminal enable/disable",
          NULL },
        { "sessiontype", 'd',
          0, G_OPTION_ARG_STRING, &session_type,
          "session type: can be one of 'unspecified', 'tty', 'x11', 'wayland' \
          or 'mir'",
          NULL },
        { "active", 'e',
          0, G_OPTION_ARG_INT, &active,
          "seat activation enable/disable",
          NULL },
        { "pamdefserv", 'f',
          0, G_OPTION_ARG_STRING, &pam_defserv,
          "pam service file for default user",
          NULL },
        { "pamserv", 'g',
          0, G_OPTION_ARG_STRING, &pam_serv,
          "pam service",
          NULL },
        { "nwatch", 'h',
          0, G_OPTION_ARG_INT, &nwatch,
          "number of seat-ready watch items",
          NULL },
        { "watchx", 'i',
          0, G_OPTION_ARG_STRING, &watchx,
          "seat-ready watch item",
          NULL },
        { "vtnr", 'j',
          0, G_OPTION_ARG_INT, &vtnr,
          "virtual terminal number for seat",
          NULL },
        { "rtimedir", 'k',
          0, G_OPTION_ARG_INT, &rtimedir,
          "setup XDG_RUNTIME_DIR for the user - enable/disable",
          NULL },
        { "rtimemode", 'm',
          0, G_OPTION_ARG_STRING, &rtimemode,
          "access mode for the XDG_RUNTIME_DIR",
          NULL },
        { NULL }
    };

    opts = g_option_context_new ("<add|change|remove> [command]");
    g_option_context_add_main_entries (opts, main_entries, NULL);
    g_option_context_parse (opts, &argc, &argv, NULL);
    g_option_context_free (opts);
    if (argc < 2) {
        printf ("%s [options] <add|change|remove> [command]\n", argv[0]);
        return -1;
    }
    if (!cf)
        cf = g_build_filename (TLM_SYSCONF_DIR, "tlm.conf", NULL);
    lockfd = open (cf, O_RDWR);
    if (lockfd < 0)
        g_critical ("open(%s): %s", cf, strerror (errno));
    if (lockf (lockfd, F_LOCK, 0))
        g_critical ("lockf(): %s", strerror (errno));
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, cf,
                                    G_KEY_FILE_KEEP_COMMENTS, &error)) {
        g_warning ("failed to load config file: %s", error->message);
        goto err_exit;
    }
    nseats = g_key_file_get_integer (kf,
                                     TLM_CONFIG_GENERAL,
                                     TLM_CONFIG_GENERAL_NSEATS,
                                     NULL);

    if (g_strcmp0 (argv[1], "add") == 0) {
        seatx = nseats;
        nseats++;
        sname = g_strdup_printf ("seat%d", seatx);
        puts (sname);
    }
    else if (g_strcmp0 (argv[1], "change") == 0) {
        if (!sname) {
            g_warning ("no seat specified");
            goto err_exit;
        }
        if (!g_key_file_has_group (kf, sname)) {
            g_warning ("given seat doesn't exist");
            goto err_exit;
        }
    }
    else if (g_strcmp0 (argv[1], "remove") == 0) {
        if (!sname) {
            g_warning ("no seat specified");
            goto err_exit;
        }
        if (!g_key_file_has_group (kf, sname)) {
            g_warning ("given seat doesn't exist");
            goto err_exit;
        }
        g_key_file_remove_key (kf,
                               sname,
                               TLM_CONFIG_GENERAL_SESSION_CMD,
                               NULL);
        autologin = 0;
        pause = 1;
    }

    if (argc >= 3) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_SESSION_CMD,
                               argv[2]);
    }
    if (username) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_DEFAULT_USER,
                               username);
    }
    if (autologin >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_GENERAL_AUTO_LOGIN,
                                autologin);
    }
    if (pause >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_GENERAL_PAUSE_SESSION,
                                pause);
    }
    if (setupterm >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_GENERAL_SETUP_TERMINAL,
                                setupterm);
    }
    if (pam_serv) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_PAM_SERVICE,
                               pam_serv);
    }
    if (pam_defserv) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_DEFAULT_PAM_SERVICE,
                               pam_defserv);
    }
    if (active >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_SEAT_ACTIVE,
                                active);
    }
    if (session_type) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_SESSION_TYPE,
                               session_type);
    }
    if (nwatch >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_SEAT_NWATCH,
                                nwatch);
    }
    if (watchx) {
        gchar *kname = g_strdup_printf ("%s%d",
                                        TLM_CONFIG_SEAT_WATCHX,
                                        nwatch - 1);
        g_key_file_set_string (kf,
                               sname,
                               kname,
                               watchx);
        g_free (kname);
    }
    if (vtnr >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_SEAT_VTNR,
                                vtnr);
    }
    if (rtimedir >= 0) {
        g_key_file_set_integer (kf,
                                sname,
                                TLM_CONFIG_GENERAL_SETUP_RUNTIME_DIR,
                                rtimedir);
    }
    if (rtimemode) {
        g_key_file_set_string (kf,
                               sname,
                               TLM_CONFIG_GENERAL_RUNTIME_MODE,
                               rtimemode);
    }
    g_key_file_set_integer (kf,
                            TLM_CONFIG_GENERAL,
                            TLM_CONFIG_GENERAL_NSEATS,
                            nseats);
    if (!g_key_file_save_to_file (kf, cf, &error))
        g_warning ("failed to save config file: %s", error->message);
    retval = 0;

err_exit:
    g_free (sname);
    g_free (cf);
    g_free (session_type);
    g_free (pam_defserv);
    g_free (pam_serv);
    g_free (watchx);
    g_free (rtimemode);
    g_key_file_free (kf);
    if (!lockf (lockfd, F_ULOCK, 0)) {}
    close (lockfd);

    return retval;
}

