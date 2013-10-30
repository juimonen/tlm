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

#include "tlm-session.h"
#include "tlm-auth-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"

G_DEFINE_TYPE (TlmSession, tlm_session, G_TYPE_OBJECT);

#define TLM_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION, TlmSessionPrivate)

enum {
    PROP_0,
    PROP_SERVICE,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

struct _TlmSessionPrivate
{
    gchar *service;
    GHashTable *env_hash;
    TlmAuthSession *auth_session;
};

static void
tlm_session_dispose (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);
    DBG("disposing session: %s", session->priv->service);

    g_clear_object (&session->priv->auth_session);
    if (session->priv->env_hash) {
        g_hash_table_unref (session->priv->env_hash);
        session->priv->env_hash = NULL;
    }

    G_OBJECT_CLASS (tlm_session_parent_class)->dispose (self);
}

static void
tlm_session_finalize (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);

    g_clear_string (&session->priv->service);

    G_OBJECT_CLASS (tlm_session_parent_class)->finalize (self);
}

static void
_session_set_property (GObject *obj,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
    TlmSession *session = TLM_SESSION(obj);

    switch (property_id) {
        case PROP_SERVICE: 
        session->priv->service = g_value_dup_string (value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_session_get_property (GObject *obj,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
    TlmSession *session = TLM_SESSION(obj);

    switch (property_id) {
        case PROP_SERVICE: 
        g_value_set_string (value, session->priv->service);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_session_class_init (TlmSessionClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof(TlmSessionPrivate));

    g_klass->dispose = tlm_session_dispose ;
    g_klass->finalize = tlm_session_finalize;
    g_klass->set_property = _session_set_property;
    g_klass->get_property = _session_get_property;

    pspecs[PROP_SERVICE] = g_param_spec_string ("service", 
                        "authentication service", "Service", NULL, 
                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);
}

static void
tlm_session_init (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);
    
    priv->service = NULL;
    priv->env_hash = g_hash_table_new_full (
                        g_str_hash, g_str_equal, g_free, g_free);
    priv->auth_session = NULL;

    session->priv = priv;
}

gboolean
tlm_session_putenv (TlmSession *session, const gchar *var, const gchar *val)
{
    g_return_val_if_fail (session && TLM_IS_SESSION (session), FALSE);
    if (!session->priv->auth_session) {
        g_hash_table_insert (session->priv->env_hash, 
                                g_strdup (var), g_strdup (val));
        return TRUE;
    }

    return tlm_auth_session_putenv (session->priv->auth_session, var, val);
}

static void
_putenv_to_auth_session (gpointer key, gpointer val, gpointer user_data)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (user_data);

    tlm_auth_session_putenv(auth_session, (const gchar*)key, (const gchar*)val);
}

static void
_session_on_auth_error (
    TlmAuthSession *session, 
    GError *error, 
    gpointer userdata)
{
    WARN ("ERROR : %s", error->message);
}

static void
_session_on_session_error(
    TlmAuthSession *session, 
    GError *error, 
    gpointer userdata)
{
    WARN ("ERROR : %s", error->message);
}

static void
_session_on_session_opened(
    TlmAuthSession *session,
    const gchar *id,
    gpointer userdata)
{
    DBG ("Session ID : %s", id);
}
gboolean
tlm_session_start (TlmSession *session, const gchar *username)
{
    g_return_val_if_fail (session && TLM_IS_SESSION(session), FALSE);
    g_return_val_if_fail (username, FALSE);

    (void)username;

    session->priv->auth_session = 
        tlm_auth_session_new (session->priv->service, username);

    if (!session->priv->auth_session) return FALSE;

    g_signal_connect (session->priv->auth_session, "auth-error", 
                G_CALLBACK(_session_on_auth_error), (gpointer)session);
    g_signal_connect (session->priv->auth_session, "session-opened",
                G_CALLBACK(_session_on_session_opened), (gpointer)session);
    g_signal_connect (session->priv->auth_session, "session-error",
                G_CALLBACK (_session_on_session_error), (gpointer)session);

    g_hash_table_foreach (session->priv->env_hash, 
                          _putenv_to_auth_session,
                          session->priv->auth_session);

    return tlm_auth_session_start (session->priv->auth_session);
}

gboolean
tlm_session_stop (TlmSession *session)
{
    g_return_val_if_fail (session && TLM_IS_SESSION(session), FALSE);

    return TRUE;
}

TlmSession *
tlm_session_new (const gchar *service)
{
    return g_object_new (TLM_TYPE_SESSION, "service", service, NULL);
}

