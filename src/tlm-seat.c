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

#include <unistd.h>
#include <fcntl.h>

#include "tlm-seat.h"
#include "tlm-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"
#include "tlm-config-general.h"

G_DEFINE_TYPE (TlmSeat, tlm_seat, G_TYPE_OBJECT);

#define TLM_SEAT_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SEAT, TlmSeatPrivate)

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_ID,
    PROP_PATH,
    PROP_DEFAULT_SERVICE,
    PROP_DEFAULT_USER,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_PREPARE_USER,
    SIG_SESSION_TERMINATED,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmSeatPrivate
{
    TlmConfig *config;
    gchar *id;
    gchar *path;
    gchar *default_service;
    gchar *default_user;
    gchar *next_service;
    gchar *next_user;
    gchar *next_password;
    TlmSession *session;
    gint notify_fd[2];
    GIOChannel *notify_channel;
};

static void
tlm_seat_dispose (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);

    DBG("disposing seat: %s", seat->priv->id);

    g_clear_object (&seat->priv->session);

    G_OBJECT_CLASS (tlm_seat_parent_class)->dispose (self);
}

static void
_reset_next (TlmSeatPrivate *priv)
{
    g_clear_string (&priv->next_service);
    g_clear_string (&priv->next_user);
    g_clear_string (&priv->next_password);
}

static void
tlm_seat_finalize (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    g_clear_string (&priv->id);
    g_clear_string (&priv->path);
    g_clear_string (&priv->default_service);
    g_clear_string (&priv->default_user);

    _reset_next (priv);

    g_io_channel_unref (priv->notify_channel);
    close (priv->notify_fd[0]);
    close (priv->notify_fd[1]);

    if (priv->config)
        g_object_unref (priv->config);

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
        case PROP_DEFAULT_SERVICE:
            priv->default_service = g_value_dup_string (value);
            break;
        case PROP_DEFAULT_USER:
            priv->default_user = g_value_dup_string (value);
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
        case PROP_DEFAULT_SERVICE:
            g_value_set_string (value, priv->default_service);
            break;
        case PROP_DEFAULT_USER:
            g_value_set_string (value, priv->default_user);
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
    pspecs[PROP_DEFAULT_SERVICE] =
        g_param_spec_string ("default-service",
                             "default service",
                             "Default PAM service for auto-login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_DEFAULT_USER] =
        g_param_spec_string ("default-user",
                             "default user name",
                             "Default user name for auto-login",
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
}

static gboolean
_notify_handler (GIOChannel *channel,
                 GIOCondition condition,
                 gpointer user_data)
{
    TlmSeat *seat = TLM_SEAT(user_data);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);
    pid_t notify_pid = 0;
    gboolean cont = TRUE;

    if (read (priv->notify_fd[0],
              &notify_pid, sizeof (notify_pid)) < (ssize_t) sizeof (notify_pid))
        WARN ("failed to read child pid for seat %p", seat);

    DBG ("handling session termination for pid %u", notify_pid);
    g_clear_object (&priv->session);

    g_signal_emit (seat,
                   signals[SIG_SESSION_TERMINATED],
                   0,
                   priv->id,
                   &cont);
    if (!cont) {
        g_clear_object (&seat->priv->session);
        return cont;
    }

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_AUTO_LOGIN,
                                TRUE)) {
        if (!priv->next_user)
            g_signal_emit (seat,
                           signals[SIG_PREPARE_USER],
                           0,
                           priv->default_user);
        tlm_seat_create_session (seat,
                                 seat->priv->next_service,
                                 seat->priv->next_user,
                                 seat->priv->next_password);
    } else {
        g_clear_object (&seat->priv->session);
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
                      const gchar *password)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);

    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);

    if (!priv->session) {
        return tlm_seat_create_session (seat, service, username, password);
    }

    if (priv->next_service)
        g_free (priv->next_service);
    priv->next_service = g_strdup (service);
    if (priv->next_user)
        g_free (priv->next_user);
    priv->next_user = g_strdup (username);
    if (priv->next_password)
        g_free (priv->next_password);
    tlm_session_terminate (priv->session);

    return TRUE;
}

gboolean
tlm_seat_create_session (TlmSeat *seat,
                         const gchar *service,
                         const gchar *username,
                         const gchar *password)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    g_return_val_if_fail (seat->priv->session == NULL, FALSE);

    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);

    if (!priv->session) {
        priv->session =
            tlm_session_new (priv->config,
                             (service) ? service : priv->default_service,
                             seat->priv->notify_fd[1],
                             (username) ? username : priv->default_user,
                             password,
                             priv->id);
        _reset_next (priv);
    }
    if (!seat->priv->session)
        return FALSE;

    return TRUE;
}

gboolean
tlm_seat_terminate_session (TlmSeat *seat)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    g_return_val_if_fail (seat->priv, FALSE);

    if (!seat->priv->session)
        return FALSE;
    tlm_session_terminate (seat->priv->session);

    return TRUE;
}


// FIXME: remove default service/user, these can be read from config
TlmSeat *
tlm_seat_new (TlmConfig *config,
              const gchar *id,
              const gchar *path,
              const gchar *default_service,
              const gchar *default_user)
{
    return g_object_new (TLM_TYPE_SEAT,
                         "config", config,
                         "id", id,
                         "path", path,
                         "default-service", default_service,
                         "default-user", default_user,
                         NULL);
}

