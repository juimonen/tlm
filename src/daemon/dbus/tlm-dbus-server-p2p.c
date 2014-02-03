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
#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common/dbus/tlm-dbus.h"
#include "common/tlm-log.h"

#include "tlm-dbus-server-p2p.h"
#include "tlm-dbus-server-interface.h"
#include "tlm-dbus-login-adapter.h"

enum
{
    PROP_0,

    IFACE_PROP_TYPE,
    PROP_ADDRESS,

    N_PROPERTIES
};

enum {
    SIG_CLIENT_ADDED,
    SIG_CLIENT_REMOVED,

    SIG_MAX
};

static guint signals[SIG_MAX];

struct _TlmDbusServerP2PPrivate
{
    GHashTable *login_object_adapters;
    GDBusServer *bus_server;
    gchar *address;
    uid_t uid;
};

static void
_tlm_dbus_server_p2p_interface_init (
        TlmDbusServerInterface *iface);

gboolean
_tlm_dbus_server_p2p_stop (
        TlmDbusServer *self);

static void
_on_connection_closed (
        GDBusConnection *connection,
        gboolean remote_peer_vanished,
        GError *error,
        gpointer user_data);

G_DEFINE_TYPE_WITH_CODE (TlmDbusServerP2P,
        tlm_dbus_server_p2p, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (TLM_TYPE_DBUS_SERVER,
                _tlm_dbus_server_p2p_interface_init));

#define TLM_DBUS_SERVER_P2P_GET_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj),\
        TLM_TYPE_DBUS_SERVER_P2P, TlmDbusServerP2PPrivate)

static void
_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    TlmDbusServerP2P *self = TLM_DBUS_SERVER_P2P (object);
    switch (property_id) {
        case IFACE_PROP_TYPE: {
            break;
        }
        case PROP_ADDRESS: {
            self->priv->address = g_value_dup_string (value);
            break;
        }
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
    TlmDbusServerP2P *self = TLM_DBUS_SERVER_P2P (object);

    switch (property_id) {
        case IFACE_PROP_TYPE: {
            g_value_set_uint (value, (guint)TLM_DBUS_SERVER_BUSTYPE_P2P);
            break;
        }
        case PROP_ADDRESS: {
            g_value_set_string (value, g_dbus_server_get_client_address (
                    self->priv->bus_server));
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gboolean
_compare_by_pointer (
        gpointer key,
        gpointer value,
        gpointer dead_object)
{
    return value == dead_object;
}

static void
_on_login_object_dispose (
        gpointer data,
        GObject *dead)
{
    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (data);
    g_return_if_fail (server);
    g_hash_table_foreach_steal (server->priv->login_object_adapters,
                _compare_by_pointer, dead);
}

static void
_clear_login_object_watchers (
        gpointer connection,
        gpointer login_object,
        gpointer user_data)
{
    g_signal_handlers_disconnect_by_func (connection, _on_connection_closed,
            user_data);
    g_object_weak_unref (G_OBJECT(login_object), _on_login_object_dispose,
            user_data);
}

static void
_add_login_object_watchers (
        gpointer connection,
        gpointer login_object,
        TlmDbusServerP2P *server)
{
    g_signal_connect (connection, "closed", G_CALLBACK(_on_connection_closed),
            server);
    g_object_weak_ref (G_OBJECT (login_object), _on_login_object_dispose,
            server);
    g_hash_table_insert (server->priv->login_object_adapters, connection,
            login_object);
}

static void
_dispose (
        GObject *object)
{
    TlmDbusServerP2P *self = TLM_DBUS_SERVER_P2P (object);

    _tlm_dbus_server_p2p_stop (TLM_DBUS_SERVER (self));

    G_OBJECT_CLASS (tlm_dbus_server_p2p_parent_class)->dispose (object);
}

static void
_finalize (
        GObject *object)
{
    TlmDbusServerP2P *self = TLM_DBUS_SERVER_P2P (object);
    if (self->priv->address) {
        if (g_str_has_prefix (self->priv->address, "unix:path=")) {
            const gchar *path = g_strstr_len(self->priv->address, -1,
                    "unix:path=") + 10;
            if (path) {
                g_unlink (path);
            }
        }
        g_free (self->priv->address);
        self->priv->address = NULL;
    }
    G_OBJECT_CLASS (tlm_dbus_server_p2p_parent_class)->finalize (object);
}

static void
tlm_dbus_server_p2p_class_init (
        TlmDbusServerP2PClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GParamSpec *address_spec = NULL;

    g_type_class_add_private (object_class, sizeof (TlmDbusServerP2PPrivate));

    object_class->get_property = _get_property;
    object_class->set_property = _set_property;
    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

    g_object_class_override_property (object_class, IFACE_PROP_TYPE, "bustype");

    address_spec = g_param_spec_string ("address", "server address",
            "Server socket address",  NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_ADDRESS, address_spec);

    signals[SIG_CLIENT_ADDED] = g_signal_new ("client-added",
            TLM_TYPE_DBUS_SERVER_P2P,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            TLM_TYPE_LOGIN_ADAPTER);

    signals[SIG_CLIENT_REMOVED] = g_signal_new ("client-removed",
            TLM_TYPE_DBUS_SERVER_P2P,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            TLM_TYPE_LOGIN_ADAPTER);
}

static void
tlm_dbus_server_p2p_init (
        TlmDbusServerP2P *self)
{
    self->priv = TLM_DBUS_SERVER_P2P_GET_PRIV(self);
    self->priv->bus_server = NULL;
    self->priv->address = NULL;
    self->priv->uid = 0;
    self->priv->login_object_adapters = g_hash_table_new_full (g_direct_hash,
            g_direct_equal, NULL, g_object_unref);
}

static void
_on_connection_closed (
        GDBusConnection *connection,
        gboolean remote_peer_vanished,
        GError *error,
        gpointer user_data)
{
    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (user_data);
    gpointer login_object = g_hash_table_lookup (
            server->priv->login_object_adapters, connection);
    if  (login_object) {
        _clear_login_object_watchers (connection, login_object, user_data);
        DBG("p2p dbus connection(%p) closed (peer vanished : %d)"
                " with error: %s", connection, remote_peer_vanished,
                error ? error->message : "NONE");

        g_signal_emit (server, signals[SIG_CLIENT_REMOVED], 0, login_object);
        g_hash_table_remove (server->priv->login_object_adapters, connection);
    }
}

static void
_tlm_dbus_server_p2p_add_login_obj (
        TlmDbusServerP2P *server,
        GDBusConnection *connection)
{
    TlmDbusLoginAdapter *login_object = NULL;

    DBG("export interfaces on connection %p", connection);

    login_object = tlm_dbus_login_adapter_new_with_connection (
            g_object_ref (connection));

    _add_login_object_watchers (connection, login_object, server);
    g_signal_emit (server, signals[SIG_CLIENT_ADDED], 0, login_object);
}

static gboolean
_on_client_request (
        GDBusServer *dbus_server,
        GDBusConnection *connection,
        gpointer user_data)
{
    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (user_data);
    if (!server) {
        WARN ("memory corruption");
        return TRUE;
    }
    _tlm_dbus_server_p2p_add_login_obj (server, connection);
    return TRUE;
}

gboolean
_tlm_dbus_server_p2p_start (
        TlmDbusServer *self)
{
    g_return_val_if_fail (TLM_IS_DBUS_SERVER_P2P (self), FALSE);

    DBG("start P2P DBus Server");

    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (self);
    if (!server->priv->bus_server) {
        GError *err = NULL;
        gchar *guid = g_dbus_generate_guid ();
        server->priv->bus_server = g_dbus_server_new_sync (
                server->priv->address, G_DBUS_SERVER_FLAGS_NONE, guid, NULL,
                NULL, &err);
        g_free (guid);

        if (!server->priv->bus_server) {
            WARN ("Failed to start server at address '%s':%s",
                    server->priv->address, err ? err->message : "NULL");
            g_error_free (err);
            return FALSE;
        }

        g_signal_connect (server->priv->bus_server, "new-connection",
                G_CALLBACK(_on_client_request), server);
    }

    if (!g_dbus_server_is_active (server->priv->bus_server)) {
        const gchar *path = NULL;
        g_dbus_server_start (server->priv->bus_server);
        path = g_strstr_len(server->priv->address, -1, "unix:path=") + 10;
        if (path) {
            if (chown (path, server->priv->uid, -1) < 0) {
                WARN("Unable to set ownership");
            }
            g_chmod (path, S_IRUSR | S_IWUSR);
        }
    }
    DBG("dbus server started at : %s", server->priv->address);

    return TRUE;
}

gboolean
_tlm_dbus_server_p2p_stop (
        TlmDbusServer *self)
{
    g_return_val_if_fail (TLM_IS_DBUS_SERVER_P2P (self), FALSE);

    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (self);

    if (server->priv->login_object_adapters) {
        DBG("cleanup watchers");
        g_hash_table_foreach (server->priv->login_object_adapters,
                _clear_login_object_watchers, server);
        g_hash_table_unref (server->priv->login_object_adapters);
        server->priv->login_object_adapters = NULL;
    }

    if (server->priv->bus_server) {
        DBG("stop P2P DBus Server");
        if (g_dbus_server_is_active (server->priv->bus_server))
            g_dbus_server_stop (server->priv->bus_server);
        g_object_unref (server->priv->bus_server);
        server->priv->bus_server = NULL;
    }

    return TRUE;
}

static pid_t
_tlm_dbus_server_p2p_get_remote_pid (
        TlmDbusServer *self,
        GDBusMethodInvocation *invocation)
{
    pid_t remote_pid = 0;
    GDBusConnection *connection = NULL;
    gint peer_fd = -1;
    struct ucred peer_cred;
    socklen_t cred_size = sizeof(peer_cred);

    g_return_val_if_fail (invocation && TLM_IS_DBUS_SERVER_P2P (self),
            remote_pid);

    connection = g_dbus_method_invocation_get_connection (invocation);
    peer_fd = g_socket_get_fd (g_socket_connection_get_socket (
            G_SOCKET_CONNECTION (g_dbus_connection_get_stream(connection))));
    if (peer_fd < 0 || getsockopt (peer_fd, SOL_SOCKET, SO_PEERCRED,
            &peer_cred, &cred_size) != 0) {
        WARN ("getsockopt() for SO_PEERCRED failed");
        return remote_pid;
    }
    DBG ("remote p2p peer pid=%d uid=%d gid=%d", peer_cred.pid, peer_cred.uid,
            peer_cred.gid);

    return peer_cred.pid;
}

static void
_tlm_dbus_server_p2p_interface_init (
        TlmDbusServerInterface *iface)
{
    iface->start = _tlm_dbus_server_p2p_start;
    iface->stop = _tlm_dbus_server_p2p_stop;
    iface->get_remote_pid = _tlm_dbus_server_p2p_get_remote_pid;
}

TlmDbusServerP2P *
tlm_dbus_server_p2p_new (
        const gchar *address,
        uid_t uid)
{
    DBG("create P2P DBus Server");

    TlmDbusServerP2P *server = TLM_DBUS_SERVER_P2P (
        g_object_new (TLM_TYPE_DBUS_SERVER_P2P, "address", address, NULL));

    if (!server || uid < 0) {
        return NULL;
    }
    server->priv->uid = uid;

    if (g_str_has_prefix(address, "unix:path=")) {
        const gchar *file_path = g_strstr_len (address, -1, "unix:path=") + 10;

        if (g_file_test(file_path, G_FILE_TEST_EXISTS)) {
            g_unlink (file_path);
        }
        gchar *base_path = g_path_get_dirname (file_path);

        if (g_mkdir_with_parents (base_path,
                S_IRUSR |S_IWUSR |S_IXUSR |S_IXGRP |S_IXOTH) == -1) {
            WARN ("Could not create '%s', error: %s", base_path,
                    strerror(errno));
            g_object_unref (server);
            server = NULL;
        }
        g_free (base_path);
    }

    return server;
}
