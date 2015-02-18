/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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

#ifndef _TLM_SESSION_H
#define _TLM_SESSION_H

#include <glib-object.h>

#include "common/tlm-config.h"

G_BEGIN_DECLS

#define TLM_TYPE_SESSION       (tlm_session_get_type())
#define TLM_SESSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_SESSION, TlmSession))
#define TLM_IS_SESSION(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_SESSION))
#define TLM_SESSION_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_SESSION))
#define TLM_SESSION_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_SESSION))

typedef struct _TlmSession TlmSession;
typedef struct _TlmSessionClass TlmSessionClass;
typedef struct _TlmSessionPrivate TlmSessionPrivate;

struct _TlmSession
{
    GObject parent;
    /* Private */
    TlmSessionPrivate *priv;
};

struct _TlmSessionClass
{
    GObjectClass parent_class;
};

GType tlm_session_get_type(void);

TlmSession *
tlm_session_new ();

gboolean
tlm_session_start (TlmSession *session,
                   const gchar *seat_id, const gchar *service,
                   const gchar *username, const gchar *password,
                   GHashTable *environment);
void
tlm_session_terminate (TlmSession *session);

G_END_DECLS

#endif /* _TLM_SESSION_H */
