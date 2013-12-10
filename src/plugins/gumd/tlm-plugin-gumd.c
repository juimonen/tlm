/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
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

#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include <gum/gum-user.h>
#include <gum/common/gum-user-types.h>
#include <gum/common/gum-file.h>

#include "tlm-plugin-gumd.h"
#include "tlm-log.h"

static gboolean
_setup_guest_account (
        TlmAccountPlugin *plugin,
        const gchar *user_name)
{
    g_return_val_if_fail (plugin && TLM_IS_ACCOUNT_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_create_sync ();
    if (!guser) {
        WARN ("Failed user %s creation", user_name);
        return FALSE;
    }

    g_object_set (G_OBJECT (guser), "usertype", GUM_USERTYPE_GUEST, "username",
            user_name, NULL);

    if (!gum_user_add_sync (guser)) {
        WARN ("Failed user %s add", user_name);
        g_object_unref (guser);
        return FALSE;
    }

    g_object_unref (guser);

    return TRUE;
}

static gboolean
_cleanup_guest_user (
        TlmAccountPlugin *plugin,
        const gchar *user_name,
        gboolean delete)
{
    uid_t uid = 0;
    gid_t gid = 0;
    gchar *home_dir = NULL;
    GError *error = NULL;
    gboolean ret = FALSE;
    guint umask = 022;

    (void) delete;

    g_return_val_if_fail (plugin && TLM_IS_ACCOUNT_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_get_by_name_sync (user_name);
    if (!guser) {
        WARN ("Failed to cleanup user %s", user_name);
        return FALSE;
    }

    g_object_get (G_OBJECT (guser), "uid", &uid, "gid", &gid, "homedir",
            &home_dir, NULL);

    if (!gum_file_delete_home_dir (home_dir, &error)) {
        goto _finished;
    }

    if (!gum_file_create_home_dir (home_dir, uid, gid, umask, &error)) {
        goto _finished;
    }
    ret = TRUE;

_finished:

    if (error) g_error_free (error);
    g_free (home_dir);
    g_object_unref (guser);

    return ret;
}

static gboolean
_is_valid_user (
        TlmAccountPlugin *plugin,
        const gchar *user_name)
{
    g_return_val_if_fail (plugin && TLM_IS_ACCOUNT_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_get_by_name_sync (user_name);
    if (!guser) {
        WARN ("Failed to find user %s", user_name);
        return FALSE;
    }

    g_object_unref (guser);
    return TRUE;
}

static void
_plugin_interface_init (
        TlmAccountPluginInterface *iface)
{
    iface->setup_guest_user_account = _setup_guest_account;
    iface->cleanup_guest_user = _cleanup_guest_user;
    iface->is_valid_user = _is_valid_user;
}

G_DEFINE_TYPE_WITH_CODE (
        TlmAccountPluginGumd,
        tlm_account_plugin_gumd,
        G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (TLM_TYPE_ACCOUNT_PLUGIN,
                _plugin_interface_init));


static void
tlm_account_plugin_gumd_class_init (
        TlmAccountPluginGumdClass *kls)
{
    (void)kls;
}

static void
tlm_account_plugin_gumd_init (
        TlmAccountPluginGumd *self)
{
    tlm_log_init(G_LOG_DOMAIN);
}

