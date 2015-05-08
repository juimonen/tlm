/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2013-2014 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"

#include "tlm-seat.h"
#include "tlm-session-remote.h"
#include "tlm-log.h"
#include "tlm-error.h"
#include "tlm-utils.h"
#include "tlm-config-general.h"
#include "tlm-dbus-observer.h"

G_DEFINE_TYPE (TlmSeat, tlm_seat, G_TYPE_OBJECT);

#define TLM_SEAT_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SEAT, TlmSeatPrivate)

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_ID,
    PROP_PATH,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_PREPARE_USER_LOGIN,
    SIG_PREPARE_USER_LOGOUT,
    SIG_SESSION_CREATED,
    SIG_SESSION_TERMINATED,
    SIG_SESSION_ERROR,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmSeatPrivate
{
    TlmConfig *config;
    gchar *id;
    gchar *default_user;
    gchar *path;
    gchar *next_service;
    gchar *next_user;
    gchar *next_password;
    GHashTable *next_environment;
    gint64 prev_time;
    gint32 prev_count;
    gboolean default_active;
    TlmSessionRemote *session;
    TlmDbusObserver *dbus_observer; /* dbus server accessed only by user who has
    active session */
    TlmDbusObserver *prev_dbus_observer;
};

typedef struct _DelayClosure
{
    TlmSeat *seat;
    gchar *service;
    gchar *username;
    gchar *password;
    GHashTable *environment;
} DelayClosure;

static void
_disconnect_session_signals (
        TlmSeat *seat);

static void
_reset_next (TlmSeatPrivate *priv)
{
    g_clear_string (&priv->next_service);
    g_clear_string (&priv->next_user);
    g_clear_string (&priv->next_password);
    if (priv->next_environment) {
        g_hash_table_unref (priv->next_environment);
        priv->next_environment = NULL;
    }
}

static void
_handle_session_created (
        TlmSeat *self,
        const gchar *sessionid,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SEAT (self));

    DBG ("sessionid: %s", sessionid);

    g_signal_emit (self, signals[SIG_SESSION_CREATED], 0, self->priv->id);

    g_clear_object (&self->priv->prev_dbus_observer);
}

static void
_close_active_session (TlmSeat *self)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (self);
    _disconnect_session_signals (self);
    if (priv->session)
        g_clear_object (&priv->session);
}

static void
_handle_session_terminated (
        TlmSeat *self,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SEAT (self));

    TlmSeat *seat = TLM_SEAT(self);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);
    gboolean stop = FALSE;

    DBG ("seat %p session %p", self, priv->session);
    _close_active_session (seat);

    g_signal_emit (seat,
            signals[SIG_SESSION_TERMINATED],
            0,
            priv->id,
            &stop);
    if (stop) {
        DBG ("no relogin or switch user");
        return;
    }
    g_clear_object (&priv->dbus_observer);

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_X11_SESSION,
                                FALSE)) {
        DBG ("X11 session termination");
        if (kill (0, SIGTERM))
            WARN ("Failed to send TERM signal to process tree");
        return;
    }

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_AUTO_LOGIN,
                                TRUE) ||
                                seat->priv->next_user) {
        DBG ("auto re-login with '%s'", seat->priv->next_user);
        tlm_seat_create_session (seat,
                seat->priv->next_service,
                seat->priv->next_user,
                seat->priv->next_password,
                seat->priv->next_environment);
        _reset_next (priv);
    }
}

static void
_handle_error (
        TlmSeat *self,
        GError *error,
        gpointer user_data)
{
    g_return_if_fail (self && TLM_IS_SEAT (self));

    DBG ("Error : %d:%s", error->code, error->message);
    g_signal_emit (self, signals[SIG_SESSION_ERROR],  0, error->code);

    if (error->code == TLM_ERROR_PAM_AUTH_FAILURE ||
        error->code == TLM_ERROR_SESSION_CREATION_FAILURE ||
        error->code == TLM_ERROR_SESSION_TERMINATION_FAILURE) {
        DBG ("Destroy the session in case of creation/termination failure");
        _close_active_session (self);
        g_clear_object (&self->priv->dbus_observer);
    }
}

static void
_disconnect_session_signals (
        TlmSeat *seat)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);
    if (!priv->session) return;
    DBG ("seat %p session %p", seat, priv->session);

    g_signal_handlers_disconnect_by_func (G_OBJECT (priv->session),
            _handle_session_created, seat);
    g_signal_handlers_disconnect_by_func (G_OBJECT (priv->session),
            _handle_session_terminated, seat);
    g_signal_handlers_disconnect_by_func (G_OBJECT (priv->session),
            _handle_error, seat);
}

static void
_connect_session_signals (
        TlmSeat *seat)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);
    DBG ("seat %p", seat);
    /* Connect session signals to handlers */
    g_signal_connect_swapped (priv->session, "session-created",
            G_CALLBACK (_handle_session_created), seat);
    g_signal_connect_swapped (priv->session, "session-terminated",
            G_CALLBACK(_handle_session_terminated), seat);
    g_signal_connect_swapped (priv->session, "session-error",
            G_CALLBACK(_handle_error), seat);
}

static gboolean
_create_dbus_observer (
        TlmSeat *seat,
        const gchar *username)
{
    gchar *address = NULL;
    uid_t uid = 0;

    if (!username) return FALSE;

    uid = tlm_user_get_uid (username);
    if (uid == -1) return FALSE;

    address = g_strdup_printf ("unix:path=%s/%s-%d", TLM_DBUS_SOCKET_PATH,
            seat->priv->id, uid);
    seat->priv->dbus_observer = TLM_DBUS_OBSERVER (tlm_dbus_observer_new (
            NULL, seat, address, uid,
            DBUS_OBSERVER_ENABLE_LOGOUT_USER |
            DBUS_OBSERVER_ENABLE_SWITCH_USER));
    g_free (address);
    DBG ("created dbus obs: %p", seat->priv->dbus_observer);
    return (seat->priv->dbus_observer != NULL);
}

static void
tlm_seat_dispose (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);

    DBG("disposing seat: %s", seat->priv->id);

    g_clear_object (&seat->priv->dbus_observer);
    g_clear_object (&seat->priv->prev_dbus_observer);

    _disconnect_session_signals (seat);
    if (seat->priv->session)
        g_clear_object (&seat->priv->session);
    if (seat->priv->config) {
        g_object_unref (seat->priv->config);
        seat->priv->config = NULL;
    }

    G_OBJECT_CLASS (tlm_seat_parent_class)->dispose (self);
}

static void
tlm_seat_finalize (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    g_clear_string (&priv->id);
    g_clear_string (&priv->default_user);
    g_clear_string (&priv->path);

    _reset_next (priv);

    G_OBJECT_CLASS (tlm_seat_parent_class)->finalize (self);
}

static void
_seat_set_property (GObject *obj,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
    TlmSeat *seat = TLM_SEAT(obj);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    switch (property_id) {
        case PROP_CONFIG:
            priv->config = g_value_dup_object (value);
            break;
        case PROP_ID: 
            priv->id = g_value_dup_string (value);
            break;
        case PROP_PATH:
            priv->path = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_seat_get_property (GObject *obj,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
    TlmSeat *seat = TLM_SEAT(obj);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    switch (property_id) {
        case PROP_CONFIG:
            g_value_set_object (value, priv->config);
            break;
        case PROP_ID: 
            g_value_set_string (value, priv->id);
            break;
        case PROP_PATH:
            g_value_set_string (value, priv->path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_seat_class_init (TlmSeatClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (TlmSeatPrivate));

    g_klass->dispose = tlm_seat_dispose ;
    g_klass->finalize = tlm_seat_finalize;
    g_klass->set_property = _seat_set_property;
    g_klass->get_property = _seat_get_property;

    pspecs[PROP_CONFIG] =
        g_param_spec_object ("config",
                             "config object",
                             "Configuration object",
                             TLM_TYPE_CONFIG,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|
                             G_PARAM_STATIC_STRINGS);
    pspecs[PROP_ID] =
        g_param_spec_string ("id",
                             "seat id",
                             "Seat Id",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|
                             G_PARAM_STATIC_STRINGS);
    pspecs[PROP_PATH] =
        g_param_spec_string ("path",
                             "object path",
                             "Seat Object path at logind",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|
                             G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_PREPARE_USER_LOGIN] = g_signal_new ("prepare-user-login",
                                              TLM_TYPE_SEAT,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              NULL,
                                              G_TYPE_NONE,
                                              1,
                                              G_TYPE_STRING);
    signals[SIG_PREPARE_USER_LOGOUT] = g_signal_new ("prepare-user-logout",
                                              TLM_TYPE_SEAT,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              NULL,
                                              G_TYPE_NONE,
                                              1,
                                              G_TYPE_STRING);
    signals[SIG_SESSION_CREATED] = g_signal_new ("session-created",
                                                    TLM_TYPE_SEAT,
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    G_TYPE_STRING);
    signals[SIG_SESSION_TERMINATED] = g_signal_new ("session-terminated",
                                                    TLM_TYPE_SEAT,
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_BOOLEAN,
                                                    1,
                                                    G_TYPE_STRING);
    signals[SIG_SESSION_ERROR] = g_signal_new ("session-error",
                                                    TLM_TYPE_SEAT,
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
tlm_seat_init (TlmSeat *seat)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);
    
    priv->id = priv->path = priv->default_user = NULL;
    priv->dbus_observer = priv->prev_dbus_observer = NULL;
    priv->default_active = FALSE;
    seat->priv = priv;
}

const gchar *
tlm_seat_get_id (TlmSeat *seat)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT (seat), NULL);

    return (const gchar*) seat->priv->id;
}

gboolean
tlm_seat_switch_user (TlmSeat *seat,
                      const gchar *service,
                      const gchar *username,
                      const gchar *password,
                      GHashTable *environment)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);

    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);

    // If username & its password is not authenticated, immediately return FALSE
    // so that current session is not terminated.
    if (!tlm_authenticate_user (priv->config, username, password)) {
        WARN("fail to tlm_authenticate_user");
        return FALSE;
    }

    if (!priv->session) {
        return tlm_seat_create_session (seat, service, username, password,
                environment);
    }

    _reset_next (priv);
    priv->next_service = g_strdup (service);
    priv->next_user = g_strdup (username);
    priv->next_password = g_strdup (password);
    priv->next_environment = g_hash_table_ref (environment);

    return tlm_seat_terminate_session (seat);
}

static gchar *
_build_user_name (const gchar *template, const gchar *seat_id)
{
    int seat_num = 0;
    const char *pptr;
    gchar *out;
    GString *str;

    if (strncmp (seat_id, "seat", 4) == 0)
        seat_num = atoi (seat_id + 4);
    else
        WARN ("Unrecognized seat id format");
    pptr = template;
    str = g_string_sized_new (16);
    while (*pptr != '\0') {
        if (*pptr == '%') {
            pptr++;
            switch (*pptr) {
                case 'S':
                    g_string_append_printf (str, "%d", seat_num);
                    break;
                case 'I':
                    g_string_append (str, seat_id);
                    break;
                default:
                    ;
            }
        } else {
            g_string_append_c (str, *pptr);
        }
        pptr++;
    }
    out = g_string_free (str, FALSE);
    return out;
}

static gboolean
_delayed_session (gpointer user_data)
{
    DelayClosure *delay_closure = (DelayClosure *) user_data;

    DBG ("delayed relogin for closure %p", delay_closure);
    g_return_val_if_fail (user_data, G_SOURCE_REMOVE);

    DBG ("delayed relogin for seat=%s, service=%s, user=%s",
         delay_closure->seat->priv->id,
         delay_closure->service,
         delay_closure->username);
    tlm_seat_create_session (delay_closure->seat,
                             delay_closure->service,
                             delay_closure->username,
                             delay_closure->password,
                             delay_closure->environment);
    g_object_unref (delay_closure->seat);
    g_free (delay_closure->service);
    g_free (delay_closure->username);
    g_free (delay_closure->password);
    if (delay_closure->environment)
        g_hash_table_unref (delay_closure->environment);
    g_slice_free (DelayClosure, delay_closure);
    return G_SOURCE_REMOVE;
}

gboolean
tlm_seat_create_session (TlmSeat *seat,
                         const gchar *service,
                         const gchar *username,
                         const gchar *password,
                         GHashTable *environment)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);

    if (priv->session != NULL) {
        g_signal_emit (seat, signals[SIG_SESSION_ERROR],  0,
                TLM_ERROR_SESSION_ALREADY_EXISTS);
        return FALSE;
    }

    if (g_get_monotonic_time () - priv->prev_time < 1000000) {
        DBG ("short time relogin");
        priv->prev_time = g_get_monotonic_time ();
        priv->prev_count++;
        if (priv->prev_count > 3) {
            WARN ("relogins spinning too fast, delay...");
            DelayClosure *delay_closure = g_slice_new0 (DelayClosure);
            delay_closure->seat = g_object_ref (seat);
            delay_closure->service = g_strdup (service);
            delay_closure->username = g_strdup (username);
            delay_closure->password = g_strdup (password);
            if (environment)
                delay_closure->environment = g_hash_table_ref (environment);
            g_timeout_add_seconds (10, _delayed_session, delay_closure);
            return TRUE;
        }
    } else {
        priv->prev_time = g_get_monotonic_time ();
        priv->prev_count = 1;
    }

    if (!service) {
        DBG ("PAM service not defined, looking up configuration");
        service = tlm_config_get_string (priv->config,
                                         priv->id,
                                         username ? TLM_CONFIG_GENERAL_PAM_SERVICE : TLM_CONFIG_GENERAL_DEFAULT_PAM_SERVICE);
        if (!service)
            service = tlm_config_get_string (priv->config,
                                             TLM_CONFIG_GENERAL,
                                             username ? TLM_CONFIG_GENERAL_PAM_SERVICE : TLM_CONFIG_GENERAL_DEFAULT_PAM_SERVICE);
        if (!service)
            service = username ? "tlm-login" : "tlm-default-login";
    }
    DBG ("using PAM service %s for seat %s", service, priv->id);

    if (!username) {
        if (!priv->default_user) {
            const gchar *name_tmpl =
                    tlm_config_get_string_default (priv->config,
                            priv->id,
                            TLM_CONFIG_GENERAL_DEFAULT_USER,
                            "guest");
            if (!name_tmpl)
                name_tmpl = tlm_config_get_string_default (priv->config,
                        TLM_CONFIG_GENERAL,
                        TLM_CONFIG_GENERAL_DEFAULT_USER,
                        "guest");
            if (name_tmpl)
                priv->default_user = _build_user_name (name_tmpl, priv->id);
        }
        if (priv->default_user) {
            priv->default_active = TRUE;
            g_signal_emit (seat,
                           signals[SIG_PREPARE_USER_LOGIN],
                           0,
                           priv->default_user);
        }
    }

    priv->session = tlm_session_remote_new (priv->config,
            priv->id,
            service,
            priv->default_active ? priv->default_user : username);
    if (!priv->session) {
        g_signal_emit (seat, signals[SIG_SESSION_ERROR], 0,
                TLM_ERROR_SESSION_CREATION_FAILURE);
        return FALSE;
    }

    /*It is needed to handle switch user case which completes after new session
     *is created */
    seat->priv->prev_dbus_observer = seat->priv->dbus_observer;
    seat->priv->dbus_observer = NULL;
    if (!_create_dbus_observer (seat,
            priv->default_active ? priv->default_user : username)) {
        g_clear_object (&priv->session);
        g_signal_emit (seat, signals[SIG_SESSION_ERROR],  0,
                TLM_ERROR_DBUS_SERVER_START_FAILURE);
        return FALSE;
    }

    _connect_session_signals (seat);
    tlm_session_remote_create (priv->session, password, environment);
    return TRUE;
}

gboolean
tlm_seat_terminate_session (TlmSeat *seat)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    g_return_val_if_fail (seat->priv, FALSE);

    if (seat->priv->default_active) {
        seat->priv->default_active = FALSE;
        g_signal_emit (seat,
                signals[SIG_PREPARE_USER_LOGOUT],
                0,
                seat->priv->default_user);
    }

    if (!seat->priv->session ||
        !tlm_session_remote_terminate (seat->priv->session)) {
        WARN ("No active session to terminate");
        g_signal_emit (seat, signals[SIG_SESSION_ERROR], 0,
                TLM_ERROR_SESSION_NOT_VALID);
        return FALSE;
    }

    return TRUE;
}

TlmSeat *
tlm_seat_new (TlmConfig *config,
              const gchar *id,
              const gchar *path)
{
    TlmSeat *seat = g_object_new (TLM_TYPE_SEAT,
                         "config", config,
                         "id", id,
                         "path", path,
                         NULL);
    return seat;
}

