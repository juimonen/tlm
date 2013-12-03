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

static GMainLoop *main_loop = NULL;
static GError *op_error = NULL;

static void
_stop_mainloop ()
{
    if (main_loop) {
        g_main_loop_quit (main_loop);
    }
}

static void
_delete_mainloop ()
{
    _stop_mainloop ();
    if (main_loop) {
        g_main_loop_unref (main_loop);
        main_loop = NULL;
    }
}

static void
_create_mainloop ()
{
    if (main_loop == NULL) {
        main_loop = g_main_loop_new (NULL, FALSE);
    }
}

static void
_run_mainloop ()
{
    _create_mainloop ();
     g_main_loop_run (main_loop);
}

static void
_on_user_op (
        GumUser *user,
        const GError *error,
        gpointer user_data)
{
    _delete_mainloop ();

    if (error) {
        op_error = g_error_copy (error);
    }
}

static gboolean
_setup_guest_account (
        TlmPlugin *plugin,
        const gchar *user_name)
{
    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_create (_on_user_op, NULL);
    if (!guser) {
        WARN ("Failed user %s creation", user_name);
        return FALSE;
    }

    _run_mainloop ();

    if (op_error) {
        WARN ("Failed user %s creation with error  %d:%s", user_name,
                op_error->code, op_error->message);
        g_object_unref (guser);
        g_error_free (op_error);
        return FALSE;
    }

    g_object_set (G_OBJECT (guser), "usertype", GUM_USERTYPE_GUEST, NULL);
    g_object_set (G_OBJECT (guser), "username", user_name, NULL);

    if (!gum_user_add (guser, _on_user_op, NULL)) {
        WARN ("Failed user %s add", user_name);
        g_object_unref (guser);
        return FALSE;
    }

    _run_mainloop ();

    g_object_unref (guser);

    if (op_error) {
        WARN ("Failed user %s add -- %d:%s", user_name, op_error->code,
                op_error->message);
        g_error_free (op_error);
        return FALSE;
    }

    return TRUE;
}

static gboolean
_cleanup_guest_user (
        TlmPlugin *plugin,
        const gchar *user_name,
        gboolean delete)
{
    uid_t uid = 0;
    gid_t gid = 0;
    gchar *home_dir = NULL;
    GError *error = NULL;
    gboolean ret = FALSE;

    (void) delete;

    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_get_by_name (user_name, _on_user_op, NULL);
    if (!guser) {
        WARN ("Failed to cleanup user %s", user_name);
        return FALSE;
    }

    _run_mainloop ();

    if (op_error) {
        WARN ("Failed to cleanup user %s %d:%s", user_name, op_error->code,
                op_error->message);
        goto _finished;
    }

    g_object_get (G_OBJECT (guser), "uid", &uid, "gid", &gid, "homedir",
            &home_dir, NULL);

    if (!gum_file_delete_home_dir (home_dir, &error)) {
        goto _finished;
    }

    if (!gum_file_create_home_dir (home_dir, uid, gid, 022, &error)) {
        goto _finished;
    }
    ret = TRUE;

_finished:

    if (op_error) g_error_free (op_error);
    if (error) g_error_free (error);
    g_free (home_dir);
    g_object_unref (guser);

    return ret;
}

static gboolean
_is_valid_user (
        TlmPlugin *plugin,
        const gchar *user_name)
{
    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_GUMD(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    GumUser *guser = gum_user_get_by_name (user_name, _on_user_op, NULL);
    if (!guser) {
        WARN ("Failed to find user %s", user_name);
        return FALSE;
    }

    _run_mainloop ();

    g_object_unref (guser);

    if (op_error) {
        WARN ("Failed user %s find -- %d:%s", user_name, op_error->code,
                op_error->message);
        g_error_free (op_error);
        return FALSE;
    }

    return TRUE;
}

static void
_plugin_interface_init (
        TlmPluginInterface *iface)
{
    iface->setup_guest_user_account = _setup_guest_account;
    iface->cleanup_guest_user = _cleanup_guest_user;
    iface->is_valid_user = _is_valid_user;
}

G_DEFINE_TYPE_WITH_CODE (
        TlmPluginGumd,
        tlm_plugin_gumd,
        G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (TLM_TYPE_PLUGIN,
                _plugin_interface_init));


enum {
    PROP_0,
    PROP_TYPE
};

static void
_get_property (
        GObject *object,
        guint prop_id,
        GValue *value,
        GParamSpec *pspec)
{
    switch (prop_id)
    {
        case PROP_TYPE:
            g_value_set_enum (value, TLM_PLUGIN_TYPE_ACCOUNT);
            break;
        gumd:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
tlm_plugin_gumd_class_init (
        TlmPluginGumdClass *kls)
{
    GObjectClass *g_kls = G_OBJECT_CLASS (kls);

    g_kls->get_property = _get_property;

    g_object_class_override_property (g_kls, PROP_TYPE, "plugin-type");
}

static void
tlm_plugin_gumd_init (
        TlmPluginGumd *self)
{
    tlm_log_init(G_LOG_DOMAIN);
}

