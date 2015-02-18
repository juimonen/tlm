/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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

/**
 * SECTION:tlm-auth-plugin
 * @short_description: an interface for implementing tlm authentication plugins
 * @include: tlm-auth-plugin.h
 *
 * #TlmAuthPlugin is an interface for implementing tlm authentication plugins.
 *
 * <refsect1><title>The plugin API</title></refsect1>
 *
 * Tlm authentication plugins provide authentication events: when they detect a user's
 * intention to login, they issue a signal, containing user's credentials. Tlm
 * acts upon this signal, by checking the credentials and starting a new session.
 *
 * <refsect1><title>Example plugins</title></refsect1>
 *
 * See example plugin implementation here:
 * <ulink url="https://github.com/01org/tlm/tree/master/src/plugins/default">
 * https://github.com/01org/tlm/tree/master/src/plugins/default</ulink> and here:
 * <ulink url="https://github.com/01org/tlm/tree/master/src/plugins/nfc">
 * https://github.com/01org/tlm/tree/master/src/plugins/nfc</ulink>.
 *
 */


/**
 * TlmAuthPluginInterface:
 * @parent: parent interface type.
 *
 * #TlmAuthPluginInterface interface.
 */

/**
 * TlmAuthPlugin:
 *
 * Opaque #TlmAuthPlugin data structure.
 */

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
     * TlmAuthPlugin::authenticate:
     * @plugin: the plugin which emitted the signal
     * @seat_id: seat id
     * @pam_service: pam service to be used
     * @user_name: username to authenticate
     * @password: password to use
     *
     * The signal is issued by the plugin, when a new authentication session should be started.
     */
    _signals[AUTHENTICATE] = g_signal_new ("authenticate",
            G_TYPE_FROM_CLASS (g_class), G_SIGNAL_RUN_LAST,
            0, NULL, NULL, NULL, G_TYPE_BOOLEAN,
            4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /**
     * TlmAuthPlugin:config:
     *
     * This property holds a list of key-value pairs of plugin configuration
     * taken from tlm.conf configuration file.
     */
    g_object_interface_install_property (g_class, g_param_spec_boxed (
            "config", "Config", "Config parameters",
            G_TYPE_HASH_TABLE, G_PARAM_CONSTRUCT_ONLY
                | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

/**
 * tlm_auth_plugin_start_authentication:
 * @self: a plugin instance
 * @seat_id: seat id
 * @pam_service: PAM service
 * @user_name: user name
 * @password: password
 *
 * This method should be used by plugin implementations to issue TlmAuthPlugin::authenticate
 * signal.
 */
gboolean
tlm_auth_plugin_start_authentication (TlmAuthPlugin   *self,
                                      const gchar *seat_id,
                                      const gchar *pam_service,
                                      const gchar *user_name,
                                      const gchar *password)
{
    gboolean res = FALSE;

    g_signal_emit(self, _signals[AUTHENTICATE], 0, 
            seat_id, pam_service, user_name, password, &res);

    return res;
}

