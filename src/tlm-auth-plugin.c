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

#include "tlm-auth-plugin.h"

G_DEFINE_INTERFACE (TlmAuthPlugin, tlm_auth_plugin, 0)

enum {
    AUTHENTICATE,
    N_SIGNALS,
};

static guint _signals[N_SIGNALS] = { 0 };

static void
tlm_auth_plugin_default_init (TlmAuthPluginInterface *g_class)
{
    /**
     * TlmAuthPlugin:authenticate:
     * @plugin: the plugin which emitted the signal
     * @seat: seat id
     * @service: pam service to be used
     * @username: username to authenticate
     * @password: password to use
     *
     * The signal is used by the plugin, when it needs to start a new 
     * authentication session on a seat
     * 
     */
    _signals[AUTHENTICATE] = g_signal_new ("authenticate",
            G_TYPE_FROM_CLASS (g_class), G_SIGNAL_RUN_LAST,
            0, NULL, NULL, NULL, G_TYPE_BOOLEAN,
            4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /**
     * TlmAuthPlugin:config:
     * 
     * This property holds a list of key-value pairs of plugin configuration
     */
    g_object_interface_install_property (g_class, g_param_spec_boxed (
            "config", "Config", "Config parameters",
            G_TYPE_HASH_TABLE, G_PARAM_CONSTRUCT_ONLY
                | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

gboolean
tlm_auth_plugin_start_authentication (TlmAuthPlugin   *self,
                                      const gchar *seat,
                                      const gchar *pam_service,
                                      const gchar *user_name,
                                      const gchar *password)
{
    gboolean res = FALSE;

    g_signal_emit(self, _signals[AUTHENTICATE], 0, 
            seat, pam_service, user_name, password, &res);

    return res;
}

