/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Alexander Kanavin <alex.kanavin@gmail.com>
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

#include <glib.h>

#include "tlm-auth-plugin-nfc.h"
#include "tlm-log.h"
#include <gtlm-nfc.h>

/**
 * SECTION:tlm-auth-plugin-nfc
 * @short_description: a NFC authentication plugin
 *
 * #TlmAuthPluginNfc issues a #TlmAuthPlugin::authenticate signal when a
 * user taps an NFC tag with his username and password on an NFC reader.
 * The plugin is utilizing
 * <ulink url="https://github.com/01org/libtlm-nfc">
 * libtlm-nfc</ulink> for NFC functionality.
 */

/**
 * TlmAuthPluginNfc:
 *
 * Opaque data structure
 */
/**
 * TlmAuthPluginNfcClass:
 * @parent_class: a reference to a parent class
 *
 * The class structure for the #TlmAuthPluginNfc objects,
 */


enum {
    PROP_0,
    PROP_CONFIG
};

struct _TlmAuthPluginNfc
{
    GObject parent;
    /* priv */

    GTlmNfc* tlm_nfc;
};


static void _plugin_interface_init (TlmAuthPluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TlmAuthPluginNfc, tlm_auth_plugin_nfc,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TLM_TYPE_AUTH_PLUGIN,
                                                _plugin_interface_init));


static void
_plugin_interface_init (TlmAuthPluginInterface *iface)
{
    (void) iface;
}

static void
_plugin_dispose (GObject *self)
{
    TlmAuthPluginNfc *plugin = TLM_AUTH_PLUGIN_NFC(self);

    if (plugin->tlm_nfc) {
        g_object_unref(plugin->tlm_nfc);
        plugin->tlm_nfc = NULL;
    }

    G_OBJECT_CLASS (tlm_auth_plugin_nfc_parent_class)->dispose(self);
}

static void
_plugin_finalize (GObject *self)
{
    TlmAuthPluginNfc *plugin = TLM_AUTH_PLUGIN_NFC(self);


    G_OBJECT_CLASS (tlm_auth_plugin_nfc_parent_class)->finalize(self);
}

static void
_plugin_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
    TlmAuthPluginNfc *self = TLM_AUTH_PLUGIN_NFC (object);

    switch (prop_id) {
        case PROP_CONFIG: {
            gpointer p = g_value_get_boxed (value);
            if (p) {
                //do nothing; we have no use for config ATM
                //self->config = g_hash_table_ref ((GHashTable *)p);
            }
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
    TlmAuthPluginNfc *self = TLM_AUTH_PLUGIN_NFC (object);

    switch (prop_id) {
        case PROP_CONFIG:
            //we don't use the config ATM
            //g_value_set_boxed (value, self->config);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
tlm_auth_plugin_nfc_class_init (TlmAuthPluginNfcClass *kls)
{
    GObjectClass *g_class = G_OBJECT_CLASS (kls);

    g_class->set_property = _plugin_set_property;
    g_class->get_property = _plugin_get_property;
    g_class->dispose = _plugin_dispose;
    g_class->finalize = _plugin_finalize;

    g_object_class_override_property (g_class, PROP_CONFIG, "config");
}

static void _nfc_record_found(GTlmNfc* tlm_nfc,
                              const gchar* username,
                              const gchar* password,
                              gpointer user_data)
{
    TlmAuthPluginNfc *plugin = TLM_AUTH_PLUGIN_NFC(user_data);

    DBG("NFC tag contains username %s password %s", username, password);
    if (!tlm_auth_plugin_start_authentication (
                TLM_AUTH_PLUGIN(plugin),
                //FIXME: how to map NFC adapters to seats?
                "seat0",
                //FIXME: what is the signficance of this?
                "tlm-login",
                username,
                password)) {
        WARN ("Authentication with an NFC tag failed");
    }
}

static void
tlm_auth_plugin_nfc_init (TlmAuthPluginNfc *self)
{
    self->tlm_nfc = g_object_new(G_TYPE_TLM_NFC, NULL);
    g_signal_connect(self->tlm_nfc, "record-found", G_CALLBACK(_nfc_record_found), self);

}

