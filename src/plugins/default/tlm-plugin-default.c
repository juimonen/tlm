/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include "tlm-plugin-default.h"
#include "tlm-log.h"

static gboolean
_setup_guest_account (TlmPlugin *plugin, const gchar *user_name)
{
    gchar *command = NULL;
    int res;
    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_DEFAULT(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    command = g_strdup_printf ("useradd %s", user_name);
    res = system (command);

    g_free (command);

    return res != -1;
}

static gboolean
_cleanup_guest_user (TlmPlugin *plugin, const gchar *user_name, gboolean delete)
{
    struct passwd *pwd_entry = NULL;
    char *command = NULL;
    int res;

    (void) delete;

    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_DEFAULT(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    /* clear error */
    errno = 0;

    pwd_entry = getpwnam (user_name);

    if (!pwd_entry) {
        DBG("Could not get info for user '%s', error : %s", user_name, strerror(errno));
        return FALSE;
    }

    if (!pwd_entry->pw_dir) {
        DBG("No home folder entry found for user '%s'", user_name);
        return FALSE;
    }

    command = g_strdup_printf ("rm -rf %s/*", pwd_entry->pw_dir);

    res = system (command);

    g_free (command);

    return res != -1;
}

static gboolean
_is_valid_user (TlmPlugin *plugin, const gchar *user_name)
{
    struct passwd *pwd_entry = NULL;

    g_return_val_if_fail (plugin && TLM_IS_PLUGIN_DEFAULT(plugin), FALSE);
    g_return_val_if_fail (user_name && user_name[0], FALSE);

    /* clear error */
    errno = 0;

    pwd_entry = getpwnam (user_name);

    if (!pwd_entry) {
        DBG("Could not get info for user '%s', error : %s", user_name, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static void
_plugin_interface_init (TlmPluginInterface *iface)
{
    iface->setup_guest_user_account = _setup_guest_account;
    iface->cleanup_guest_user = _cleanup_guest_user;
    iface->is_valid_user = _is_valid_user;
}

G_DEFINE_TYPE_WITH_CODE (TlmPluginDefault, tlm_plugin_default,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TLM_TYPE_PLUGIN,
                                                _plugin_interface_init));


enum {
    PROP_0,
    PROP_TYPE
};

static void
_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    switch (prop_id)
    {
        case PROP_TYPE:
            g_value_set_enum (value, TLM_PLUGIN_TYPE_ACCOUNT);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
tlm_plugin_default_class_init (TlmPluginDefaultClass *kls)
{
    GObjectClass *g_kls = G_OBJECT_CLASS (kls);

    g_kls->get_property = _get_property;

    g_object_class_override_property (g_kls, PROP_TYPE, "plugin-type");
}

static void
tlm_plugin_default_init (TlmPluginDefault *self)
{
    tlm_log_init(G_LOG_DOMAIN);
}

