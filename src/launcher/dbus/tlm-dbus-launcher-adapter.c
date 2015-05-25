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

#include "tlm-dbus-launcher-adapter.h"

enum
{
    PROP_0,

    PROP_CONNECTION,

    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _TlmDbusLauncherAdapterPrivate
{
    GDBusConnection *connection;
    TlmDbusLauncherObserver *observer;
    TlmDbusLauncher *dbus_obj;
};

G_DEFINE_TYPE (TlmDbusLauncherAdapter, tlm_dbus_launcher_adapter, G_TYPE_OBJECT)

#define TLM_DBUS_LAUNCHER_ADAPTER_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_LAUNCHER_ADAPTER, \
            TlmDbusLauncherAdapterPrivate)

enum {
    SIG_PROCESS_LAUNCHED,
    SIG_PROCESS_TERMINATED,

    SIG_MAX
};

static guint signals[SIG_MAX];

static gboolean
_handle_launch_process (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *command,
        const gchar *args,
        gpointer emitter);

static gboolean
_handle_stop_process (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        guint32 procid,
        gpointer emitter);

static gboolean
_handle_list_processes (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer emitter);

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    TlmDbusLauncherAdapter *self = TLM_DBUS_LAUNCHER_ADAPTER (object);

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
    TlmDbusLauncherAdapter *self = TLM_DBUS_LAUNCHER_ADAPTER (object);

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
    TlmDbusLauncherAdapter *self = TLM_DBUS_LAUNCHER_ADAPTER (object);

    DBG("- unregistering dbus launcher adaptor %p.", self);

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

    if (self->priv->observer) {
    	g_object_unref (self->priv->observer);
    	self->priv->observer = NULL;
    }

    G_OBJECT_CLASS (tlm_dbus_launcher_adapter_parent_class)->dispose (
            object);
}

static void
_finalize (
        GObject *object)
{

    G_OBJECT_CLASS (tlm_dbus_launcher_adapter_parent_class)->finalize (
            object);
}

static void
tlm_dbus_launcher_adapter_class_init (
        TlmDbusLauncherAdapterClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (TlmDbusLauncherAdapterPrivate));

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

    signals[SIG_PROCESS_LAUNCHED] = g_signal_new ("process-launched",
            TLM_TYPE_LAUNCHER_ADAPTER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);

    signals[SIG_PROCESS_TERMINATED] = g_signal_new ("process-terminated",
            TLM_TYPE_LAUNCHER_ADAPTER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);
}

static void
tlm_dbus_launcher_adapter_init (TlmDbusLauncherAdapter *self)
{
    self->priv = TLM_DBUS_LAUNCHER_ADAPTER_GET_PRIV(self);

    self->priv->connection = 0;
    self->priv->observer = 0;
    self->priv->dbus_obj = tlm_dbus_launcher_skeleton_new ();
}

static gboolean
_handle_launch_process (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        const gchar *command,
        const gchar *args,
        gpointer emitter)
{
    GError *error = NULL;
    gboolean rval = FALSE;
    guint procid;
    g_return_val_if_fail (self && TLM_IS_DBUS_LAUNCHER_ADAPTER(self), FALSE);

    if (!command) {
        error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "Invalid input");
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        return TRUE;
    }
    DBG ("launch - command %s args %s", command, args);
    rval = tlm_dbus_launcher_launch_process (self->priv->observer, command,
            args, &procid, &error);
    if (rval) {
        tlm_dbus_launcher_complete_launch_process (self->priv->dbus_obj,
                invocation, procid);
    } else {
    	g_dbus_method_invocation_return_gerror (invocation, error);
    	g_error_free (error);
    }

    return TRUE;
}

static gboolean
_handle_stop_process (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        guint procid,
        gpointer emitter)
{
    GError *error = NULL;
    gboolean rval = FALSE;
    g_return_val_if_fail (self && TLM_IS_DBUS_LAUNCHER_ADAPTER(self),
            FALSE);

    if (!procid) {
        error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "Invalid proc id");
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_error_free (error);
        return TRUE;
    }
    DBG ("stopping - process %u", procid);

    rval = tlm_dbus_launcher_stop_process (self->priv->observer, procid, &error);
    if (rval) {
        tlm_dbus_launcher_complete_stop_process (self->priv->dbus_obj,
                invocation);
    } else {
    	g_dbus_method_invocation_return_gerror (invocation, error);
    	g_error_free (error);
    }

    return TRUE;
}

static gboolean
_handle_list_processes (
        TlmDbusLauncherAdapter *self,
        GDBusMethodInvocation *invocation,
        gpointer emitter)
{
    g_return_val_if_fail (self && TLM_IS_DBUS_LAUNCHER_ADAPTER(self),
            FALSE);

    DBG ("TODO: list processes");

    return TRUE;
}

TlmDbusLauncherAdapter *
tlm_dbus_launcher_adapter_new_with_connection (
		TlmDbusLauncherObserver *observer,
        GDBusConnection *bus_connection)
{
    GError *err = NULL;
    TlmDbusLauncherAdapter *adapter = TLM_DBUS_LAUNCHER_ADAPTER (g_object_new (
            TLM_TYPE_LAUNCHER_ADAPTER, "connection", bus_connection, NULL));

    adapter->priv->observer = g_object_ref (observer);
    if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON(adapter->priv->dbus_obj),
            adapter->priv->connection, TLM_LAUNCHER_OBJECTPATH, &err)) {
        WARN ("failed to register object: %s", err->message);
        g_error_free (err);
        g_object_unref (adapter);
        return NULL;
    }

    DBG("(+) started launcher interface '%p' at path '%s' on connection"
            " '%p'", adapter, TLM_LAUNCHER_OBJECTPATH, bus_connection);

    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-launch-process", G_CALLBACK (_handle_launch_process), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-stop-process", G_CALLBACK(_handle_stop_process), adapter);
    g_signal_connect_swapped (adapter->priv->dbus_obj,
        "handle-list-processes", G_CALLBACK(_handle_list_processes), adapter);

    return adapter;
}
