/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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

#ifndef _TLM_UTILS_H
#define _TLM_UTILS_H

#include <sys/types.h>
#include <glib.h>

G_BEGIN_DECLS

void
g_clear_string (gchar **);

const gchar *
tlm_user_get_name (uid_t user_id);

uid_t
tlm_user_get_uid (const gchar *username);

gid_t
tlm_user_get_gid (const gchar *username);

const gchar *
tlm_user_get_home_dir (const gchar *username);

const gchar *
tlm_user_get_shell (const gchar *username);

gboolean
tlm_utils_delete_dir (const gchar *dir);

void
tlm_utils_log_utmp_entry (const gchar *username);

gchar **
tlm_utils_split_command_line (const gchar *command);

GList *
tlm_utils_split_command_lines (const GList const *commands_list);

typedef void (*WatchCb) (const gchar *found_item, gboolean is_final, GError *error, gpointer userdata);

guint
tlm_utils_watch_for_files (const gchar **watch_list, WatchCb cb, gpointer userdata);

G_END_DECLS

#endif /* _TLM_UTILS_H */
