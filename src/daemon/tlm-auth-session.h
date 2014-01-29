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

#ifndef _TLM_AUTH_SESSION_H
#define _TLM_AUTH_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_AUTH_SESSION       (tlm_auth_session_get_type())
#define TLM_AUTH_SESSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_AUTH_SESSION, TlmAuthSession))
#define TLM_IS_AUTH_SESSION(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_AUTH_SESSION))
#define TLM_AUTH_SESSION_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_AUTH_SESSION))
#define TLM_AUTH_SESSION_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_AUTH_SESSION))


typedef enum {
    TLM_AUTH_SESSION_PAM_ERROR = 1,
} TlmAuthSessionError;

#define TLM_AUTH_SESSION_ERROR g_quark_from_string("tlm-auth-session-error")

typedef struct _TlmAuthSession TlmAuthSession;
typedef struct _TlmAuthSessionClass TlmAuthSessionClass;
typedef struct _TlmAuthSessionPrivate TlmAuthSessionPrivate;

struct _TlmAuthSession
{
    GObject parent;
    /* Private */
    TlmAuthSessionPrivate *priv;
};

struct _TlmAuthSessionClass
{
    GObjectClass parent_class;
};

GType tlm_auth_session_get_type(void);

TlmAuthSession *
tlm_auth_session_new (const gchar *service,
                      const gchar *username,
                      const gchar *password);

gboolean
tlm_auth_session_putenv (TlmAuthSession *auth_session,
                         const gchar *var,
                         const gchar *value);

gboolean
tlm_auth_session_start (TlmAuthSession *auth_session);

const gchar *
tlm_auth_session_get_username (TlmAuthSession *auth_session);

gchar **
tlm_auth_session_get_envlist (TlmAuthSession *auth_session);

G_END_DECLS

#endif /* _TLM_AUTH_SESSION_H */
