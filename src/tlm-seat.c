/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
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

G_DEFINE_TYPE (TlmSeat, tlm_seat, G_TYPE_OBJECT);

#define TLM_SEAT_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SEAT, TlmSeatPrivate)

enum {
    PROP_0,
    PROP_ID,
    PROP_PATH,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

struct _TlmSeatPrivate
{
    gchar *id;
    gchar *path;
    gchar *default_user;
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
tlm_seat_finalize (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);
    TlmSeatPrivate *priv = TLM_SEAT_PRIV(seat);

    if (priv->id) {
        g_free (priv->id);
        priv->id = NULL;
    }

    if (priv->path) {
        g_free (priv->path);
        priv->path = NULL;
    }

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

    switch (property_id) {
        case PROP_ID: 
        seat->priv->id = g_value_dup_string (value);
        break;

        case PROP_PATH:
        seat->priv->path = g_value_dup_string (value);
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

    switch (property_id) {
        case PROP_ID: 
        g_value_set_string (value, seat->priv->id);
        break;

        case PROP_PATH:
        g_value_set_string (value, seat->priv->path);
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

    pspecs[PROP_ID] = g_param_spec_string ("id", "seat id", "Seat Id", NULL, 
                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);
    pspecs[PROP_PATH] = g_param_spec_string ("path", "object path",
             "Seat Object path at logind", NULL, 
             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);
}

static gboolean
_notify_handler (GIOChannel *channel,
                 GIOCondition condition,
                 gpointer user_data)
{
    TlmSeat *seat = TLM_SEAT(user_data);
    pid_t notify_pid = 0;

    if (read (seat->priv->notify_fd[0],
              &notify_pid, sizeof (notify_pid)) < (ssize_t) sizeof (notify_pid))
        WARN ("failed to read child pid for seat %p", seat);

    DBG ("handling session termination for pid %u", notify_pid);
    g_clear_object (&seat->priv->session);

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
tlm_seat_create_session (
    TlmSeat *seat,
    const gchar *service, 
    const gchar *username)
{
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);
    g_return_val_if_fail (seat->priv->session == NULL, FALSE);
    g_return_val_if_fail (service, FALSE);

    seat->priv->session = tlm_session_new (service,
                                           seat->priv->notify_fd[1],
                                           username, NULL);
    if (!seat->priv->session)
        return FALSE;
    tlm_session_putenv (seat->priv->session, "XDG_SEAT", seat->priv->id);

    return TRUE;
}

TlmSeat *
tlm_seat_new (const gchar *id, const gchar *path)
{
    return g_object_new (TLM_TYPE_SEAT, "id", id, "path", path, NULL);
}

