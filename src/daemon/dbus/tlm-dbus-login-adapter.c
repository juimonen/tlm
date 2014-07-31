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
#include "common/tlm-error.h"
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

enum {
    SIG_LOGIN_USER,
    SIG_LOGOUT_USER,
    SIG_SWITCH_USER,

    SIG_MAX
};

static guint signals[SIG_MAX];

static gboolean
_handle_login_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        const GVariant *environ,
        gpointer user_data);

static gboolean
_handle_switch_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        const GVariant *environ,
        gpointer user_data);

static gboolean
_handle_logout_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
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
            self->priv->connection = g_value_dup_object (value);
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

    DBG("- unregistering dbus login adaptor %p.", self);

    if (self->priv->dbus_obj) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (
                self->priv->dbus_obj));
        g_object_unref (self->priv->dbus_obj);
        self->priv->dbus_obj = NULL;
    }

    if (self->priv->connection) {
        /* NOTE: There seems to be some bug in glib's dbus connection's
         * worker thread such that it does not closes the stream. The problem
         * is hard to be tracked exactly as it is more of timing issue.
         * Following code snippet at least closes the stream to avoid any
         * descriptors leak.
         * */
        GIOStream *stream = g_dbus_connection_get_stream (
                self->priv->connection);
        if (stream) g_io_stream_close (stream, NULL, NULL);
        g_object_unref (self->priv->connection);
        self->priv->connection = NULL;
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

    signals[SIG_LOGIN_USER] = g_signal_new ("login-user",
            TLM_TYPE_LOGIN_ADAPTER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            5,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_VARIANT,
            G_TYPE_DBUS_METHOD_INVOCATION);

    signals[SIG_LOGOUT_USER] = g_signal_new ("logout-user",
            TLM_TYPE_LOGIN_ADAPTER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            2,
            G_TYPE_STRING,
            G_TYPE_DBUS_METHOD_INVOCATION);

    signals[SIG_SWITCH_USER] = g_signal_new ("switch-user",
            TLM_TYPE_LOGIN_ADAPTER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            5,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_VARIANT,
            G_TYPE_DBUS_METHOD_INVOCATION);
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
        const GVariant *environment,
        gpointer emitter)
{
    GError *error = NULL;

    g_return_val_if_fail (self && TLM_IS_DBUS_LOGIN_ADAPTER(self), FALSE);

    if (!seat_id || !username || !password) {
        error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "Invalid input");
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        return TRUE;
    }
    DBG ("seat_id %s username %s", seat_id, username);

    g_signal_emit (self, signals[SIG_LOGIN_USER], 0, seat_id, username,
            password, environment, invocation);

    return TRUE;
}

static gboolean
_handle_logout_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        gpointer emitter)
{
    GError *error = NULL;

    g_return_val_if_fail (self && TLM_IS_DBUS_LOGIN_ADAPTER(self),
            FALSE);

    if (!seat_id) {
        error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "Invalid input");
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        return TRUE;
    }
    DBG ("seat_id %s", seat_id);

    g_signal_emit (self, signals[SIG_LOGOUT_USER], 0, seat_id, invocation);

    return TRUE;
}

static gboolean
_handle_switch_user (
        TlmDbusLoginAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        const GVariant *environment,
        gpointer emitter)
{
    GError *error = NULL;

    g_return_val_if_fail (self && TLM_IS_DBUS_LOGIN_ADAPTER(self),
            FALSE);

    if (!seat_id || !username || !password) {
        error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "Invalid input");
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        return TRUE;
    }
    DBG ("seat_id %s username %s", seat_id, username);

    g_signal_emit (self, signals[SIG_SWITCH_USER], 0, seat_id, username,
            password, environment, invocation);

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
        "handle-logout-user", G_CALLBACK(_handle_logout_user), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-switch-user", G_CALLBACK(_handle_switch_user), adapter);

    return adapter;
}

void
tlm_dbus_login_adapter_request_completed (
        TlmDbusRequest *request,
        GError *error)
{
    g_return_if_fail (request && request->dbus_adapter &&
            TLM_IS_DBUS_LOGIN_ADAPTER(request->dbus_adapter));

    TlmDbusLoginAdapter *adapter = TLM_DBUS_LOGIN_ADAPTER (
            request->dbus_adapter);
    if (error) {
        g_dbus_method_invocation_return_gerror (request->invocation, error);
        return;
    }

    switch (request->type) {
    case TLM_DBUS_REQUEST_TYPE_LOGIN_USER:
        tlm_dbus_login_complete_login_user (adapter->priv->dbus_obj,
                request->invocation);
        break;
    case TLM_DBUS_REQUEST_TYPE_LOGOUT_USER:
        tlm_dbus_login_complete_logout_user (adapter->priv->dbus_obj,
                request->invocation);
        break;
    case TLM_DBUS_REQUEST_TYPE_SWITCH_USER:
        tlm_dbus_login_complete_switch_user (adapter->priv->dbus_obj,
                request->invocation);
        break;
    }
}
