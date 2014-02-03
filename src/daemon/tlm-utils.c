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

#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <glib/gstdio.h>

#include "tlm-utils.h"

void
g_clear_string (gchar **str)
{
    if (str && *str) {
        g_free (*str);
        *str = NULL;
    }
}

const gchar *
tlm_user_get_name (uid_t user_id)
{
    struct passwd *pwent;

    pwent = getpwuid (user_id);
    if (!pwent)
        return NULL;

    return pwent->pw_name;
}

uid_t
tlm_user_get_uid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_uid;
}

gid_t
tlm_user_get_gid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_gid;
}

const gchar *
tlm_user_get_home_dir (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_dir;
}

const gchar *
tlm_user_get_shell (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_shell;
}

gboolean
tlm_utils_delete_dir (
        const gchar *dir)
{
    GDir* gdir = NULL;
    struct stat sent;

    if (!dir || !(gdir = g_dir_open(dir, 0, NULL))) {
        return FALSE;
    }

    const gchar *fname = NULL;
    gint retval = 0;
    gchar *filepath = NULL;
    while ((fname = g_dir_read_name (gdir)) != NULL) {
        if (g_strcmp0 (fname, ".") == 0 ||
            g_strcmp0 (fname, "..") == 0) {
            continue;
        }
        retval = -1;
        filepath = g_build_filename (dir, fname, NULL);
        if (filepath) {
            retval = lstat(filepath, &sent);
            if (retval == 0) {
                /* recurse the directory */
                if (S_ISDIR (sent.st_mode)) {
                    retval = (gint)!tlm_utils_delete_dir (filepath);
                } else {
                    retval = g_remove (filepath);
                }
            }
            g_free (filepath);
        }
        if (retval != 0) {
            g_dir_close (gdir);
            return FALSE;
        }
    }
    g_dir_close (gdir);

    if (g_remove (dir) != 0) {
        return FALSE;
    }

    return TRUE;
}
