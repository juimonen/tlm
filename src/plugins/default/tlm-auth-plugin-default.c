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
#include <signal.h>
#include <glib.h>

#include "tlm-auth-plugin-default.h"
#include "tlm-log.h"

/**
 * SECTION:tlm-auth-plugin-default
 * @short_description: an default authentication plugin
 *
 * #TlmAuthPluginDefault issues #TlmAuthPlugin::authenticate signals when it receives
 * a SIGUSR1 signal. Username and password are set to NULL, and seat is
 * set to default.
 *
 * This has the effect of logging in a guest user when TLM receives a SIGUSR1 signal.
 */

/**
 * TlmAuthPluginDefault:
 *
 * Opaque data structure
 */
/**
 * TlmAuthPluginDefaultClass:
 * @parent_class: a reference to a parent class
 *
 * The class structure for the #TlmAuthPluginDefault objects,
 */

struct _TlmAuthPluginDefault
{
    GObject parent;
    /* priv */
    GHashTable *config;
};

enum {
    PROP_0,
    PROP_CONFIG
};

GWeakRef __self ;

static void _plugin_interface_init (TlmAuthPluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TlmAuthPluginDefault, tlm_auth_plugin_default,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TLM_TYPE_AUTH_PLUGIN,
                                                _plugin_interface_init));


static void
_plugin_interface_init (TlmAuthPluginInterface *iface)
{
    (void) iface;
}

static void
_plugin_finalize (GObject *self)
{
    TlmAuthPluginDefault *plugin = TLM_AUTH_PLUGIN_DEFAULT(self);

    if (plugin->config) g_hash_table_unref (plugin->config);

    g_weak_ref_clear (&__self);

    G_OBJECT_CLASS (tlm_auth_plugin_default_parent_class)->finalize(self);
}

static void
_on_signal_cb (int sig_no)
{
    DBG("Signal '%d'", sig_no);
    TlmAuthPluginDefault *self = g_weak_ref_get (&__self);

    if (!self) return ;

    /* re-login guest on seat0 */
    if (!tlm_auth_plugin_start_authentication (
                TLM_AUTH_PLUGIN(self),
                "seat0",
                "tlm-login",
                NULL,
                NULL)) {
        WARN ("Failed to create session");
    }
}

static void
_plugin_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
    TlmAuthPluginDefault *self = TLM_AUTH_PLUGIN_DEFAULT (object);

    switch (prop_id) {
        case PROP_CONFIG: {
            gpointer p = g_value_get_boxed (value);
            if (p)
                self->config = g_hash_table_ref ((GHashTable *)p);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
_plugin_get_property (GObject *object,
                      guint    prop_id,
                      GValue  *value,
                      GParamSpec *pspec)
{
    TlmAuthPluginDefault *self = TLM_AUTH_PLUGIN_DEFAULT (object);

    switch (prop_id) {
        case PROP_CONFIG:
            g_value_set_boxed (value, self->config);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
tlm_auth_plugin_default_class_init (TlmAuthPluginDefaultClass *kls)
{
    GObjectClass *g_class = G_OBJECT_CLASS (kls);

    g_class->set_property = _plugin_set_property;
    g_class->get_property = _plugin_get_property;
    g_class->finalize  = _plugin_finalize;

    g_object_class_override_property (g_class, PROP_CONFIG, "config");
}

static void
tlm_auth_plugin_default_init (TlmAuthPluginDefault *self)
{
    struct sigaction sa = { 0 };

    g_weak_ref_init (&__self, self);

    sa.sa_handler = _on_signal_cb;
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        WARN ("assert(sigaction()) : %s", strerror(errno));
    }

    tlm_log_init("TLM_AUTH_PLIGIN");
}

