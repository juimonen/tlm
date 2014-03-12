/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
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
#include "tlm-session.h"
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
    SIG_PREPARE_USER,
    SIG_SESSION_CREATED,
    SIG_SESSION_TERMINATED,
    SIG_SESSION_ERROR,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmSeatPrivate
{
    TlmConfig *config;
    TlmManager *manager;
    gchar *id;
    gchar *path;
    gchar *next_service;
    gchar *next_user;
    gchar *next_password;
    GHashTable *next_environment;
    gint64 prev_time;
    gint32 prev_count;
    TlmSession *session;
    TlmDbusObserver *dbus_observer; /* dbus server accessed only by user who has
    active session */
    gint notify_fd[2];
    GIOChannel *notify_channel;
    guint logout_id;      /* logout source id */
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
_destroy_dbus_observer (
        TlmDbusObserver **dbus_observer)
{
    if (*dbus_observer) {
        g_object_unref (*dbus_observer);
        *dbus_observer = NULL;
    }
}

static gboolean
_create_dbus_observer (
        TlmSeat *seat,
        const gchar *username)
{
    gchar *address = NULL;
    uid_t uid = 0;

    if (!username) return FALSE;

    _destroy_dbus_observer (&seat->priv->dbus_observer);

    uid = tlm_user_get_uid (username);
    address = g_strdup_printf ("unix:path=%s/%s-%d", TLM_DBUS_SOCKET_PATH,
            seat->priv->id, uid);
    seat->priv->dbus_observer = TLM_DBUS_OBSERVER (tlm_dbus_observer_new (
            seat->priv->manager, seat, address, uid,
            DBUS_OBSERVER_ENABLE_LOGOUT_USER |
            DBUS_OBSERVER_ENABLE_SWITCH_USER));
    g_free (address);

    return (seat->priv->dbus_observer != NULL);
}

static void
tlm_seat_dispose (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);

    DBG("disposing seat: %s", seat->priv->id);

    if (seat->priv->logout_id) {
        g_source_remove (seat->priv->logout_id);
        seat->priv->logout_id = 0;
    }

    _destroy_dbus_observer (&seat->priv->dbus_observer);

    g_clear_object (&seat->priv->session);

    if (seat->priv->config) {
        g_object_unref (seat->priv->config);
        seat->priv->config = NULL;
    }

    if (seat->priv->manager) {
        g_object_unref (seat->priv->manager);
        seat->priv->manager = NULL;
    }

    G_OBJECT_CLASS (tlm_seat_parent_class)->dispose (self);
}

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
tlm_seat_finalize (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    g_clear_string (&priv->id);
    g_clear_string (&priv->path);

    _reset_next (priv);

    g_io_channel_unref (priv->notify_channel);
    close (priv->notify_fd[0]);
    close (priv->notify_fd[1]);

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
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_ID] =
        g_param_spec_string ("id",
                             "seat id",
                             "Seat Id",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_PATH] =
        g_param_spec_string ("path",
                             "object path",
                             "Seat Object path at logind",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_PREPARE_USER] = g_signal_new ("prepare-user",
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

static gboolean
_notify_handler (GIOChannel *channel,
                 GIOCondition condition,
                 gpointer user_data)
{
    TlmSeat *seat = TLM_SEAT(user_data);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);
    pid_t notify_pid = 0;
    gboolean stop = FALSE;

    if (read (priv->notify_fd[0],
              &notify_pid, sizeof (notify_pid)) < (ssize_t) sizeof (notify_pid))
        WARN ("Failed to read child pid for seat %p", seat);

    DBG ("handling session termination for pid %u", notify_pid);
    tlm_session_reset_tty (priv->session);
    g_clear_object (&priv->session);

    g_signal_emit (seat,
                   signals[SIG_SESSION_TERMINATED],
                   0,
                   priv->id,
                   &stop);
    if (stop) {
        DBG ("no relogin or switch user");
        return TRUE;
    }

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_X11_SESSION,
                                FALSE)) {
        DBG ("X11 session termination");
        if (kill (0, SIGTERM))
            WARN ("Failed to send TERM signal to process tree");
        return TRUE;
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

    return TRUE;
}

static void
tlm_seat_init (TlmSeat *seat)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);
    
    priv->id = priv->path = NULL;

    seat->priv = priv;

    if (pipe2 (priv->notify_fd, O_NONBLOCK | O_CLOEXEC))
        ERR ("pipe2() failed");
    priv->notify_channel = g_io_channel_unix_new (priv->notify_fd[0]);
    g_io_add_watch (priv->notify_channel,
                    G_IO_IN,
                    _notify_handler,
                    seat);
    priv->logout_id = 0;
    priv->manager = NULL;
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

    g_return_val_if_fail (user_data, FALSE);

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
    return FALSE;
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

    gchar *default_user = NULL;

    if (!service) {
        service = tlm_config_get_string (priv->config,
                                         priv->id,
                                         TLM_CONFIG_GENERAL_PAM_SERVICE);
        if (!service)
            service = tlm_config_get_string (priv->config,
                                             TLM_CONFIG_GENERAL,
                                             TLM_CONFIG_GENERAL_PAM_SERVICE);
    }
    if (!username) {
        const gchar *name_tmpl =
            tlm_config_get_string (priv->config,
                                   priv->id,
                                   TLM_CONFIG_GENERAL_DEFAULT_USER);
        if (!name_tmpl)
            name_tmpl = tlm_config_get_string (priv->config,
                                               TLM_CONFIG_GENERAL,
                                               TLM_CONFIG_GENERAL_DEFAULT_USER);
        if (name_tmpl) default_user = _build_user_name (name_tmpl, priv->id);
        if (default_user)
            g_signal_emit (seat,
                           signals[SIG_PREPARE_USER],
                           0,
                           default_user);
    }

    priv->session = tlm_session_new (priv->config,
            priv->id,
            service,
            default_user ? default_user : username,
            password,
            environment,
            priv->notify_fd[1]);
    if (!priv->session) {
        g_signal_emit (seat, signals[SIG_SESSION_ERROR], 0,
                TLM_ERROR_SESSION_CREATION_FAILURE);
        return FALSE;
    }

    TlmDbusObserver *old_observer = seat->priv->dbus_observer;
    seat->priv->dbus_observer = NULL;
    if (!_create_dbus_observer (seat, default_user ? default_user : username)) {
        if (tlm_seat_terminate_session (seat)) {
            g_signal_emit (seat, signals[SIG_SESSION_ERROR],  0,
                    TLM_ERROR_DBUS_SERVER_START_FAILURE);
        }
        return FALSE;
    }

    g_signal_emit (seat, signals[SIG_SESSION_CREATED], 0, priv->id);
    _destroy_dbus_observer (&old_observer);
    return TRUE;
}

gboolean
tlm_seat_terminate_session (TlmSeat *seat)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    g_return_val_if_fail (seat->priv, FALSE);

    if (!seat->priv->session) {
        WARN ("No active session to terminate");
        g_signal_emit (seat, signals[SIG_SESSION_ERROR], 0,
                TLM_ERROR_SESSION_NOT_VALID);
        return FALSE;
    }

    tlm_session_terminate (seat->priv->session);

    return TRUE;
}

TlmSeat *
tlm_seat_new (TlmConfig *config,
              TlmManager *manager,
              const gchar *id,
              const gchar *path)
{
    TlmSeat *seat = g_object_new (TLM_TYPE_SEAT,
                         "config", config,
                         "id", id,
                         "path", path,
                         NULL);
    if (seat) seat->priv->manager = g_object_ref (manager);
    return seat;
}

