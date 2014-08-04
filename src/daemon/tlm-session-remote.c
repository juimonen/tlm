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

#include <string.h>
#include <errno.h>

#include "common/tlm-log.h"
#include "common/tlm-error.h"
#include "common/tlm-config.h"
#include "common/tlm-config-general.h"
#include "common/tlm-pipe-stream.h"
#include "common/dbus/tlm-dbus.h"
#include "common/dbus/tlm-dbus-utils.h"
#include "common/dbus/tlm-dbus-session-gen.h"
#include "tlm-session-remote.h"

#define TLM_SESSIOND_NAME "tlm-sessiond"

enum
{
    PROP_0,
    PROP_CONFIG,
    PROP_SEATID,
    PROP_SERVICE,
    PROP_USERNAME,
    PROP_SESSIONID,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _TlmSessionRemotePrivate
{
	TlmConfig *config;
    GDBusConnection *connection;
    TlmDbusSession *dbus_session_proxy;
    GPid cpid;
    guint child_watch_id;
    gboolean is_sessiond_up;
    int last_sig;
    guint timer_id;
    gboolean can_emit_signal;

    /* Signals */
    gulong signal_session_created;
    gulong signal_session_terminated;
    gulong signal_authenticated;
    gulong signal_error;
};

G_DEFINE_TYPE (TlmSessionRemote, tlm_session_remote, G_TYPE_OBJECT);

#define TLM_SESSION_REMOTE_PRIV(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION_REMOTE, \
            TlmSessionRemotePrivate))
enum {
    SIG_SESSION_CREATED,
    SIG_SESSION_TERMINATED,
    SIG_AUTHENTICATED,
    SIG_SESSION_ERROR,
    SIG_MAX
};

static guint signals[SIG_MAX];

static void
_on_child_down_cb (
        GPid  pid,
        gint  status,
        gpointer data)
{
    g_spawn_close_pid (pid);

    TlmSessionRemote *session = TLM_SESSION_REMOTE (data);

    DBG ("Sessiond(%p) with pid (%d) closed with status %d", session, pid,
            status);

    session->priv->is_sessiond_up = FALSE;
    session->priv->child_watch_id = 0;
    if (session->priv->timer_id) {
        g_source_remove (session->priv->timer_id);
        session->priv->timer_id = 0;
    }
    if (session->priv->can_emit_signal)
        g_signal_emit (session, signals[SIG_SESSION_TERMINATED], 0);
}

static void
tlm_session_remote_set_property (
        GObject *object,
        guint property_id,
        const GValue *value,
        GParamSpec *pspec)
{
    TlmSessionRemote *self = TLM_SESSION_REMOTE (object);
    switch (property_id) {
    	case PROP_CONFIG:
           self->priv->config = g_value_dup_object (value);
           break;
		case PROP_SEATID:
		case PROP_USERNAME:
		case PROP_SERVICE: {
			if (self->priv->dbus_session_proxy) {
				g_object_set_property (G_OBJECT(self->priv->dbus_session_proxy),
						pspec->name, value);
			}
			break;
		}
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
tlm_session_remote_get_property (
        GObject *object,
        guint property_id,
        GValue *value,
        GParamSpec *pspec)
{
    TlmSessionRemote *self = TLM_SESSION_REMOTE (object);

    switch (property_id) {
    	case PROP_CONFIG:
    		g_value_set_object (value, self->priv->config);
    		break;
		case PROP_SEATID:
		case PROP_USERNAME:
		case PROP_SERVICE:
		case PROP_SESSIONID: {
			if (self->priv->dbus_session_proxy) {
				g_object_get_property (G_OBJECT(self->priv->dbus_session_proxy),
						pspec->name, value);
			}
			break;
		}
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

}

static gboolean
_terminate_timeout (gpointer user_data)
{
    TlmSessionRemote *self = TLM_SESSION_REMOTE(user_data);
    TlmSessionRemotePrivate *priv = TLM_SESSION_REMOTE_PRIV(self);

    switch (priv->last_sig)
    {
        case SIGHUP:
            DBG ("child %u didn't respond to SIGHUP, sending SIGTERM",
                 priv->cpid);
            if (kill (priv->cpid, SIGTERM))
                WARN ("kill(%u, SIGTERM): %s",
                      priv->cpid,
                      strerror(errno));
            priv->last_sig = SIGTERM;
            return G_SOURCE_CONTINUE;
        case SIGTERM:
            DBG ("child %u didn't respond to SIGTERM, sending SIGKILL",
                 priv->cpid);
            if (kill (priv->cpid, SIGKILL))
                WARN ("kill(%u, SIGKILL): %s",
                      priv->cpid,
                      strerror(errno));
            priv->last_sig = SIGKILL;
            return G_SOURCE_CONTINUE;
        case SIGKILL:
            DBG ("child %u didn't respond to SIGKILL, "
            		"process is stuck in kernel",  priv->cpid);
            priv->timer_id = 0;
            if (self->priv->can_emit_signal) {
                GError *error = TLM_GET_ERROR_FOR_ID (
                        TLM_ERROR_SESSION_TERMINATION_FAILURE,
                        "Unable to terminate session - process is stuck"
                        " in kernel");
                g_signal_emit (self, signals[SIG_SESSION_ERROR], 0, error);
                g_error_free (error);
            }
            return G_SOURCE_REMOVE;
        default:
            WARN ("%d has unknown signaling state %d",
                  priv->cpid,
                  priv->last_sig);
    }
    return G_SOURCE_REMOVE;
}

static void
tlm_session_remote_dispose (GObject *object)
{
    TlmSessionRemote *self = TLM_SESSION_REMOTE (object);
    self->priv->can_emit_signal = FALSE;

    DBG("self %p", self);
    if (self->priv->is_sessiond_up) {
        tlm_session_remote_terminate (self);
        while (self->priv->is_sessiond_up)
            g_main_context_iteration(NULL, TRUE);
        DBG ("Sessiond DESTROYED");
    }

    if (self->priv->timer_id) {
        g_source_remove (self->priv->timer_id);
        self->priv->timer_id = 0;
    }

    self->priv->cpid = 0;
    self->priv->last_sig = 0;

    if (self->priv->child_watch_id > 0) {
        g_source_remove (self->priv->child_watch_id);
        self->priv->child_watch_id = 0;
    }

    g_clear_object (&self->priv->config);

    if (self->priv->dbus_session_proxy) {
        g_signal_handler_disconnect (self->priv->dbus_session_proxy,
                self->priv->signal_session_created);
        g_signal_handler_disconnect (self->priv->dbus_session_proxy,
                self->priv->signal_session_terminated);
        g_signal_handler_disconnect (self->priv->dbus_session_proxy,
                self->priv->signal_error);
        g_signal_handler_disconnect (self->priv->dbus_session_proxy,
                self->priv->signal_authenticated);
        g_object_unref (self->priv->dbus_session_proxy);
        self->priv->dbus_session_proxy = NULL;
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

    DBG("done");
    G_OBJECT_CLASS (tlm_session_remote_parent_class)->dispose (object);
}

static void
tlm_session_remote_finalize (GObject *object)
{
    //TlmSessionRemote *self = TLM_SESSION_REMOTE (object);

    G_OBJECT_CLASS (tlm_session_remote_parent_class)->finalize (object);
}

static void
tlm_session_remote_class_init (TlmSessionRemoteClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class,
            sizeof (TlmSessionRemotePrivate));

    object_class->get_property = tlm_session_remote_get_property;
    object_class->set_property = tlm_session_remote_set_property;
    object_class->dispose = tlm_session_remote_dispose;
    object_class->finalize = tlm_session_remote_finalize;

    properties[PROP_CONFIG] = g_param_spec_object ("config",
                             "config object",
                             "Configuration object",
                             TLM_TYPE_CONFIG,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|
                             G_PARAM_STATIC_STRINGS);
    properties[PROP_SEATID] = g_param_spec_string ("seatid",
            "SeatId",
            "Id of the seat",
            "seat0" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);
    properties[PROP_SERVICE] = g_param_spec_string ("service",
            "Service",
            "Service",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);
    properties[PROP_USERNAME] = g_param_spec_string ("username",
            "Username",
            "Username",
            "" /* default value */,
            G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS);
    properties[PROP_SESSIONID] = g_param_spec_string ("sessionid",
            "SessionId",
            "Id of the session",
            "" /* default value */,
            G_PARAM_READABLE |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[SIG_SESSION_CREATED] = g_signal_new ("session-created",
                                TLM_TYPE_SESSION_REMOTE, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_STRING);

    signals[SIG_SESSION_TERMINATED] = g_signal_new ("session-terminated",
                                TLM_TYPE_SESSION_REMOTE, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_AUTHENTICATED] = g_signal_new ("authenticated",
                                TLM_TYPE_SESSION_REMOTE, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_SESSION_ERROR] = g_signal_new ("session-error",
                                TLM_TYPE_SESSION_REMOTE, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_ERROR);
}

static void
tlm_session_remote_init (TlmSessionRemote *self)
{
    self->priv = TLM_SESSION_REMOTE_PRIV(self);

    self->priv->connection = NULL;
    self->priv->dbus_session_proxy = NULL;
    self->priv->cpid = 0;
    self->priv->child_watch_id = 0;
    self->priv->is_sessiond_up = FALSE;
    self->priv->last_sig = 0;
    self->priv->timer_id = 0;
}

static void
_session_created_async_cb (
        GObject *object,
        GAsyncResult *res,
        gpointer user_data)
{
    GError *error = NULL;
    TlmDbusSession *proxy = TLM_DBUS_SESSION (object);
    TlmSessionRemote *self = TLM_SESSION_REMOTE (user_data);

    tlm_dbus_session_call_session_create_finish (proxy,
            res, &error);
    if (error) {
        WARN("session creation request failed");
        g_signal_emit (self, signals[SIG_SESSION_ERROR],  0, error);
        g_error_free (error);
    }
}

void
tlm_session_remote_create (
    TlmSessionRemote *session,
    const gchar *password,
    GHashTable *environment)
{
    GVariant *data = NULL;
    gchar *pass = g_strdup (password);
    if (environment) data = tlm_dbus_utils_hash_table_to_variant (environment);
    if (!data) data = g_variant_new ("a{ss}", NULL);

    if (!pass) pass = g_strdup ("");
    tlm_dbus_session_call_session_create (
            session->priv->dbus_session_proxy, pass, data, NULL,
            _session_created_async_cb, session);
    g_free (pass);
}

/* signals */
static void
_on_session_created_cb (
        TlmSessionRemote *self,
        const gchar *sessionid,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_REMOTE (self));
    DBG("sessionid: %s", sessionid ? sessionid : "NULL");
    g_signal_emit (self, signals[SIG_SESSION_CREATED], 0, sessionid);
}

static void
_on_session_terminated_cb (
        TlmSessionRemote *self,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_REMOTE (self));
    if (self->priv->can_emit_signal)
        g_signal_emit (self, signals[SIG_SESSION_TERMINATED], 0);
}

static void
_on_authenticated_cb (
        TlmSessionRemote *self,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_REMOTE (self));
    g_signal_emit (self, signals[SIG_AUTHENTICATED], 0);
}

static void
_on_error_cb (
        TlmSessionRemote *self,
        GVariant *error,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SESSION_REMOTE (self));

    GError *gerror = tlm_error_new_from_variant (error);
    WARN("error %d:%s", gerror->code, gerror->message);
    if (self->priv->can_emit_signal)
        g_signal_emit (self, signals[SIG_SESSION_ERROR],  0, gerror);
    g_error_free (gerror);
}

TlmSessionRemote *
tlm_session_remote_new (
        TlmConfig *config,
        const gchar *seat_id,
        const gchar *service,
        const gchar *username)
{
    GError *error = NULL;
    GPid cpid = 0;
    gchar **argv;
    gint cin_fd, cout_fd;
    TlmSessionRemote *session = NULL;
    TlmPipeStream *stream = NULL;
    gboolean ret = FALSE;
    const gchar *bin_path = TLM_BIN_DIR;

#   ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("TLM_BIN_DIR");
    if (env_val)
        bin_path = env_val;
#   endif

    if (!bin_path || strlen(bin_path) == 0) {
        WARN ("Invalid tlm binary path %s", bin_path?bin_path:"null");
        return NULL;
    }

    /* This guarantees that writes to a pipe will never cause
     * a process termination via SIGPIPE, and instead a proper
     * error will be returned */
    signal(SIGPIPE, SIG_IGN);

    /* Spawn child process */
    argv = g_new0 (gchar *, 1 + 1);
    argv[0] = g_build_filename (bin_path, TLM_SESSIOND_NAME, NULL);
    ret = g_spawn_async_with_pipes (NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD, NULL,
            NULL, &cpid, &cin_fd, &cout_fd, NULL, &error);
    g_strfreev (argv);
    if (ret == FALSE || (kill(cpid, 0) != 0)) {
        DBG ("failed to start sessiond: error %s(%d)",
            error ? error->message : "(null)", ret);
        if (error) g_error_free (error);
        return NULL;
    }

    /* Create dbus session object */
    session = TLM_SESSION_REMOTE (g_object_new (TLM_TYPE_SESSION_REMOTE,
            "config", config, NULL));

    session->priv->child_watch_id = g_child_watch_add (cpid,
            (GChildWatchFunc)_on_child_down_cb, session);
    session->priv->cpid = cpid;
    session->priv->is_sessiond_up = TRUE;

    /* Create dbus connection */
    stream = tlm_pipe_stream_new (cout_fd, cin_fd, TRUE);
    session->priv->connection = g_dbus_connection_new_sync (
            G_IO_STREAM (stream), NULL, G_DBUS_CONNECTION_FLAGS_NONE, NULL,
            NULL, NULL);
    g_object_unref (stream);

    /* Create dbus proxy */
    session->priv->dbus_session_proxy =
            tlm_dbus_session_proxy_new_sync (
                    session->priv->connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    TLM_SESSION_OBJECTPATH,
                    NULL,
                    &error);
    if (error) {
        DBG ("Failed to register object: %s", error->message);
        g_error_free (error);
        g_object_unref (session);
        return NULL;
    }
    DBG("'%s' object exported(%p)", TLM_SESSION_OBJECTPATH, session);

    session->priv->signal_session_created = g_signal_connect_swapped (
            session->priv->dbus_session_proxy, "session-created",
            G_CALLBACK (_on_session_created_cb), session);
    session->priv->signal_session_terminated = g_signal_connect_swapped (
            session->priv->dbus_session_proxy, "session-terminated",
            G_CALLBACK(_on_session_terminated_cb), session);
    session->priv->signal_authenticated = g_signal_connect_swapped (
            session->priv->dbus_session_proxy, "authenticated",
            G_CALLBACK(_on_authenticated_cb), session);
    session->priv->signal_error = g_signal_connect_swapped (
            session->priv->dbus_session_proxy, "error",
            G_CALLBACK(_on_error_cb), session);

    g_object_set (G_OBJECT (session), "seatid", seat_id, "service", service,
            "username", username, NULL);

    session->priv->can_emit_signal = TRUE;
    return session;
}

gboolean
tlm_session_remote_terminate (
        TlmSessionRemote *self)
{
    g_return_if_fail (self && TLM_IS_SESSION_REMOTE(self));
    TlmSessionRemotePrivate *priv = TLM_SESSION_REMOTE_PRIV(self);

    if (!priv->is_sessiond_up) {
        WARN ("sessiond is not running");
        return FALSE;
    }

    DBG ("Terminate child session process");
    if (kill (priv->cpid, SIGHUP) < 0)
        WARN ("kill(%u, SIGHUP): %s", priv->cpid, strerror(errno));
    priv->last_sig = SIGHUP;
    priv->timer_id = g_timeout_add_seconds (
            tlm_config_get_uint (priv->config, TLM_CONFIG_GENERAL,
                    TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT, 3),
            _terminate_timeout, self);
    return TRUE;
}


