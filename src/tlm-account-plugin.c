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

#include "tlm-account-plugin.h"

G_DEFINE_INTERFACE (TlmAccountPlugin, tlm_account_plugin, 0)

static void
tlm_account_plugin_default_init (TlmAccountPluginInterface *g_class)
{
    /**
     * TlmAccountPlugin:config:
     * 
     * This property holds a list of key-value pairs of plugin configuration
     */
    g_object_interface_install_property (g_class, g_param_spec_boxed (
            "config", "Config", "Config parameters",
            G_TYPE_HASH_TABLE, G_PARAM_CONSTRUCT_ONLY 
                | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

gboolean
tlm_account_plugin_setup_guest_user (TlmAccountPlugin   *self,
                                     const gchar *user_name)
{
    g_return_val_if_fail(self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail(
        TLM_ACCOUNT_PLUGIN_GET_IFACE(self)->setup_guest_user_account, FALSE);
    
    return TLM_ACCOUNT_PLUGIN_GET_IFACE (self)->setup_guest_user_account (
                    self, user_name);
}

gboolean
tlm_account_plugin_is_valid_user (TlmAccountPlugin   *self,
                                  const gchar *user_name)
{
    g_return_val_if_fail (self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail (TLM_ACCOUNT_PLUGIN_GET_IFACE(self)->is_valid_user,
                          FALSE);

    return TLM_ACCOUNT_PLUGIN_GET_IFACE(self)->is_valid_user (self, user_name);
}

gboolean
tlm_account_plugin_cleanup_guest_user (TlmAccountPlugin   *self,
                                       const gchar *user_name,
                                       gboolean     delete_user)
{
    g_return_val_if_fail(self && TLM_IS_PLUGIN (self), FALSE);
    g_return_val_if_fail(TLM_ACCOUNT_PLUGIN_GET_IFACE(self)->cleanup_guest_user,
                         FALSE);

    return TLM_ACCOUNT_PLUGIN_GET_IFACE (self)->cleanup_guest_user (
                    self, user_name, delete_user);
}

