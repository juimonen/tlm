/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "config.h"
#include "common/tlm-log.h"
#include "common/dbus/tlm-dbus.h"

#include "tlm-dbus-login-adapter.h"

enum
{
    PROP_0,

    PROP_CONNECTION,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _TlmDbusLoginAdapterPrivate
{
    GDBusConnection *connection;
    TlmDbusLogin *dbus_obj;
};

G_DEFINE_TYPE (TlmDbusLoginAdapter, tlm_dbus_login_adapter, G_TYPE_OBJECT)

#define TLM_DBUS_LOGIN_ADAPTER_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_LOGIN_ADAPTER, \
            TlmDbusLoginAdapterPrivate)

static gboolean
_handle_login_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data);

static gboolean
_handle_switch_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data);

static gboolean
_handle_logout_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data);

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    TlmDbusLoginAdapter *self = TLM_DBUS_LOGIN_ADAPTER (object);

    switch (property_id) {
        case PROP_CONNECTION:
            self->priv->connection = g_value_get_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    TlmDbusLoginAdapter *self = TLM_DBUS_LOGIN_ADAPTER (object);

    switch (property_id) {
        case PROP_CONNECTION:
            g_value_set_object (value, self->priv->connection);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
_dispose (
        GObject *object)
{
    TlmDbusLoginAdapter *self = TLM_DBUS_LOGIN_ADAPTER (object);

    DBG("- unregistering dbus login adaptor.");

    if (self->priv->dbus_obj) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (
                self->priv->dbus_obj));
        g_object_unref (self->priv->dbus_obj);
        self->priv->dbus_obj = NULL;
    }

    G_OBJECT_CLASS (tlm_dbus_login_adapter_parent_class)->dispose (
            object);
}

static void
_finalize (
        GObject *object)
{

    G_OBJECT_CLASS (tlm_dbus_login_adapter_parent_class)->finalize (
            object);
}

static void
tlm_dbus_login_adapter_class_init (
        TlmDbusLoginAdapterClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (TlmDbusLoginAdapterPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    properties[PROP_CONNECTION] = g_param_spec_object (
            "connection",
            "Bus connection",
            "DBus connection used",
            G_TYPE_DBUS_CONNECTION,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
tlm_dbus_login_adapter_init (TlmDbusLoginAdapter *self)
{
    self->priv = TLM_DBUS_LOGIN_ADAPTER_GET_PRIV(self);

    self->priv->connection = 0;
    self->priv->dbus_obj = tlm_dbus_login_skeleton_new ();
}

static gboolean
_handle_login_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data)
{
    GError *error = NULL;

    DBG ("");
    g_return_val_if_fail (self && TLM_IS_DBUS_LOGIN_ADAPTER(self),
            FALSE);


    if (1) {

    } else {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }
    
    return TRUE;
}

static gboolean
_handle_switch_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data)
{
    GError *error = NULL;
    TlmDbusLoginAdapter *dbus_user = NULL;

    DBG ("");
    if (dbus_user) {
    } else {
        if (!error) {
            //error = GUM_GET_ERROR_FOR_ID (GUM_ERROR_USER_NOT_FOUND,
            //        "User Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }


    return TRUE;
}

static gboolean
_handle_logout_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        gpointer user_data)
{
    GError *error = NULL;
    TlmDbusLoginAdapter *dbus_user = NULL;

    DBG ("");
    if (dbus_user) {
    } else {
        if (!error) {
            //error = GUM_GET_ERROR_FOR_ID (GUM_ERROR_USER_NOT_FOUND,
            //        "User Not Found");
        }
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
    }

    return TRUE;
}

TlmDbusLoginAdapter *
tlm_dbus_login_adapter_new_with_connection (
        GDBusConnection *bus_connection)
{
    GError *err = NULL;
    TlmDbusLoginAdapter *adapter = TLM_DBUS_LOGIN_ADAPTER (g_object_new (
            TLM_TYPE_LOGIN_ADAPTER, "connection", bus_connection, NULL));

    if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON(adapter->priv->dbus_obj),
            adapter->priv->connection, TLM_LOGIN_OBJECTPATH, &err)) {
        WARN ("failed to register object: %s", err->message);
        g_error_free (err);
        g_object_unref (adapter);
        return NULL;
    }
    DBG("(+) started login interface '%p' at path '%s' on connection"
            " '%p'", adapter, TLM_LOGIN_OBJECTPATH, bus_connection);

    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-login-user", G_CALLBACK (_handle_login_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-switch-user", G_CALLBACK(_handle_switch_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-logout-user", G_CALLBACK(_handle_logout_user), adapter);

    return adapter;
}
