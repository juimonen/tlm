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

#include "tlm-plugin.h"
#include "tlm-plugin-types.h"

G_DEFINE_INTERFACE (TlmPlugin, tlm_plugin, 0)

enum {
    AUTHENTICATE,
    N_SIGNALS,
};

static guint _signals[N_SIGNALS] = { 0 };

static void
tlm_plugin_default_init (TlmPluginInterface *g_class)
{
    /**
     * TlmPlugin:authenticate:
     * @plugin: the plugin which emitted the signal
     * @service: pam service to be used
     * @username: username to authenticate
     * @seat: seat id
     *
     * The signal is used by the plugin, when it needs to start a new 
     * authentication session on a seat
     * 
     */
    _signals[AUTHENTICATE] = g_signal_new ("authenticate",
            G_TYPE_FROM_CLASS (g_class), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE,
            3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /**
     * TlmPlugin:type:
     *
     * This property holds the plugin type, #TlmPlugin
     */

    g_object_interface_install_property(g_class, g_param_spec_enum (
                    "plugin-type", "Plugin Type", "Plugin type",
                    TLM_TYPE_PLUGIN_TYPE_FLAGS, TLM_PLUGIN_TYPE_ACCOUNT,
                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

TlmPluginTypeFlags
tlm_plugin_get_plugin_type (TlmPlugin *self)
{
    TlmPluginTypeFlags type;
    g_return_val_if_fail (self && TLM_IS_PLUGIN (self), TLM_PLUGIN_TYPE_NONE);

    g_object_get (G_OBJECT (self), "plugin-type", &type, NULL);

    return type;
}

gboolean
tlm_plugin_setup_guest_user (TlmPlugin   *self,
                             const gchar *user_name)
{
    g_return_val_if_fail (self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail (TLM_PLUGIN_GET_IFACE(self)->setup_guest_user_account, FALSE);
    
    return TLM_PLUGIN_GET_IFACE (self)->setup_guest_user_account (
                    self, user_name);
}

gboolean
tlm_plugin_is_valid_user (TlmPlugin   *self,
                          const gchar *user_name)
{
    g_return_val_if_fail (self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail (TLM_PLUGIN_GET_IFACE(self)->is_valid_user, FALSE);

    return TLM_PLUGIN_GET_IFACE(self)->is_valid_user (self, user_name);
}

gboolean
tlm_plugin_cleanup_guest_user (TlmPlugin   *self,
                               const gchar *user_name,
                               gboolean     delete_user)
{
    g_return_val_if_fail (self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail (TLM_PLUGIN_GET_IFACE (self)->cleanup_guest_user, FALSE);

    return TLM_PLUGIN_GET_IFACE (self)->cleanup_guest_user (
                    self, user_name, delete_user);
}

void
tlm_plugin_start_authentication (TlmPlugin   *self,
                                 const gchar *service,
                                 const gchar *user_name,
                                 const gchar *seat_id)
{
    g_signal_emit (self, _signals[AUTHENTICATE], 0, service, user_name, seat_id);
}

