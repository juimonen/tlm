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

#include "common/tlm-log.h"
#include "common/tlm-error.h"
#include "common/tlm-pipe-stream.h"
#include "common/dbus/tlm-dbus-session-gen.h"
#include "common/dbus/tlm-dbus-utils.h"
#include "common/dbus/tlm-dbus.h"
#include "tlm-session-daemon.h"
#include "tlm-session.h"

struct _TlmSessionDaemonPrivate
{
    GDBusConnection *connection;
    TlmDbusSession *dbus_session;
    TlmSession *session;
};

G_DEFINE_TYPE (TlmSessionDaemon, tlm_session_daemon, G_TYPE_OBJECT)


#define TLM_SESSION_DAEMON_GET_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION_DAEMON,\
            TlmSessionDaemonPrivate)

static void
_dispose (GObject *object)
{
    TlmSessionDaemon *self = TLM_SESSION_DAEMON (object);
    if (self->priv->dbus_session) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (
                self->priv->dbus_session));
        g_object_unref (self->priv->dbus_session);
        self->priv->dbus_session = NULL;
    }

    if (self->priv->connection) {
        g_object_unref (self->priv->connection);
        self->priv->connection = NULL;
    }

    if (self->priv->session) {
        g_object_unref (self->priv->session);
        self->priv->session = NULL;
    }

    G_OBJECT_CLASS (tlm_session_daemon_parent_class)->dispose (object);
}

static void
_finalize (GObject *object)
{
    G_OBJECT_CLASS (tlm_session_daemon_parent_class)->finalize (object);
}

static void
tlm_session_daemon_class_init (
        TlmSessionDaemonClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (
            TlmSessionDaemonPrivate));

    object_class->dispose = _dispose;
    object_class->finalize = _finalize;

}

static void
tlm_session_daemon_init (
        TlmSessionDaemon *self)
{
    self->priv = TLM_SESSION_DAEMON_GET_PRIV(self);
    self->priv->connection = NULL;
    self->priv->dbus_session = NULL;
    self->priv->session = NULL;
}

static void
_on_connection_closed (
        GDBusConnection *connection,
        gboolean         remote_peer_vanished,
        GError          *error,
        gpointer         user_data)
{
    TlmSessionDaemon *daemon = TLM_SESSION_DAEMON (user_data);

    g_signal_handlers_disconnect_by_func (connection, _on_connection_closed,
            user_data);
    DBG("dbus connection(%p) closed (peer vanished : %d)", connection,
            remote_peer_vanished);
    if (error) {
       DBG("...reason : %s", error->message);
    }
    g_object_unref (daemon);
}

static gboolean
_handle_session_create_from_dbus (
        TlmSessionDaemon *self,
        GDBusMethodInvocation *invocation,
        const gchar *password,
        GVariant *environment,
        gpointer user_data)
{
    g_return_val_if_fail (self && TLM_IS_SESSION_DAEMON (self), FALSE);
    gchar *seatid = NULL;
    gchar *service = NULL;
    gchar *username = NULL;
    GHashTable *data = NULL;

    tlm_dbus_session_complete_session_create (
            self->priv->dbus_session, invocation);

    gchar *data_str = g_variant_print(environment, TRUE);
    DBG("%s", data_str);
    g_free(data_str);

    data = tlm_dbus_utils_hash_table_from_variant (environment);
    g_object_get (self->priv->dbus_session, "seatid", &seatid,
    		"username", &username, "service", &service, NULL);

    tlm_session_start (self->priv->session, seatid, service, username,
    		password, data);

    g_hash_table_unref (data);
    g_free (seatid);
    g_free (service);
    g_free (username);
    return TRUE;
}

static gboolean
_handle_session_terminate_from_dbus (
        TlmSessionDaemon *self,
        GDBusMethodInvocation *invocation,
        gpointer user_data)
{
    g_return_val_if_fail (self && TLM_IS_SESSION_DAEMON (self), FALSE);

    tlm_dbus_session_complete_session_terminate (self->priv->dbus_session,
            invocation);

    tlm_session_terminate (self->priv->session);
    return TRUE;
}

static void
_handle_session_created_from_session (
        TlmSessionDaemon *self,
        const gchar *sessionid,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_DAEMON (self));

    DBG ("sessionid: %s", sessionid);

    g_object_set (G_OBJECT (self->priv->dbus_session), "sessionid", sessionid,
            NULL);
    tlm_dbus_session_emit_session_created (self->priv->dbus_session, sessionid);
}

static void
_handle_session_terminated_from_session (
        TlmSessionDaemon *self,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_DAEMON (self));

    tlm_dbus_session_emit_session_terminated (self->priv->dbus_session);
}

static void
_handle_authenticated_from_session (
        TlmSessionDaemon *self,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_DAEMON (self));

    tlm_dbus_session_emit_authenticated (self->priv->dbus_session);
}

static void
_handle_error_from_session (
        TlmSessionDaemon *self,
        GError *gerror,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_DAEMON (self));

    GVariant *error = tlm_error_to_variant (gerror);
    gchar *data_str = g_variant_print (error, TRUE);
    DBG("%s", data_str);
    g_free (data_str);

    tlm_dbus_session_emit_error (self->priv->dbus_session, error);
}

TlmSessionDaemon *
tlm_session_daemon_new (
        gint in_fd,
        gint out_fd)
{
    GError *error = NULL;
    TlmPipeStream *stream = NULL;

    TlmSessionDaemon *daemon = TLM_SESSION_DAEMON (g_object_new (
            TLM_TYPE_SESSION_DAEMON, NULL));

    /* Load session */
    daemon->priv->session = tlm_session_new ();
    if (!daemon->priv->session) {
        DBG ("failed to create session object");
        g_object_unref (daemon);
        return NULL;
    }
    tlm_log_init(G_LOG_DOMAIN);
    /* Create dbus connection */
    stream = tlm_pipe_stream_new (in_fd, out_fd, TRUE);
    daemon->priv->connection = g_dbus_connection_new_sync (G_IO_STREAM (stream),
            NULL, G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING, NULL, NULL,
            NULL);
    g_object_unref (stream);

    /* Create dbus object */
    daemon->priv->dbus_session =
            tlm_dbus_session_skeleton_new ();

    /* Connect dbus remote session signals to handlers */
    g_signal_connect_swapped (daemon->priv->dbus_session,
            "handle-session-create", G_CALLBACK (
                _handle_session_create_from_dbus), daemon);
    g_signal_connect_swapped (daemon->priv->dbus_session,
            "handle-session-terminate", G_CALLBACK(
                _handle_session_terminate_from_dbus), daemon);

    /* Connect session signals to handlers */
    g_signal_connect_swapped (daemon->priv->session, "session-created",
            G_CALLBACK (_handle_session_created_from_session), daemon);
    g_signal_connect_swapped (daemon->priv->session, "session-terminated",
            G_CALLBACK(_handle_session_terminated_from_session), daemon);
    g_signal_connect_swapped (daemon->priv->session, "authenticated",
            G_CALLBACK(_handle_authenticated_from_session), daemon);
    g_signal_connect_swapped (daemon->priv->session, "session-error",
            G_CALLBACK(_handle_error_from_session), daemon);

    g_signal_connect (daemon->priv->connection, "closed",
            G_CALLBACK(_on_connection_closed), daemon);

    g_dbus_interface_skeleton_export (
                G_DBUS_INTERFACE_SKELETON(daemon->priv->dbus_session),
                daemon->priv->connection, TLM_SESSION_OBJECTPATH, &error);
    if (error) {
        DBG ("failed to register object: %s", error->message);
        g_error_free (error);
        g_object_unref (daemon);
        return NULL;
    }
    DBG("Started session daemon '%p' at path '%s' on conneciton '%p'",
            daemon, TLM_SESSION_OBJECTPATH, daemon->priv->connection);

    g_dbus_connection_start_message_processing (daemon->priv->connection);

    return daemon;
}
