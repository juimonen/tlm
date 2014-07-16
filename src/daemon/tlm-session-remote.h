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

#ifndef __TLM_SESSION_REMOTE_H_
#define __TLM_SESSION_REMOTE_H_

#include <glib.h>
#include "common/tlm-config.h"

G_BEGIN_DECLS

#define TLM_TYPE_SESSION_REMOTE (tlm_session_remote_get_type())
#define TLM_SESSION_REMOTE(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj),\
    TLM_TYPE_SESSION_REMOTE, TlmSessionRemote))
#define TLM_SESSION_REMOTE_CLASS(klass)\
    (G_TYPE_CHECK_CLASS_CAST((klass), TLM_TYPE_SESSION_REMOTE, \
    TlmSessionRemoteClass))
#define TLM_IS_SESSION_REMOTE(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TLM_TYPE_SESSION_REMOTE))
#define TLM_IS_SESSION_REMOTE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TLM_TYPE_SESSION_REMOTE))
#define TLM_SESSION_REMOTE_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), TLM_TYPE_SESSION_REMOTE, \
    TlmSessionRemoteClass))

typedef struct _TlmSessionRemote TlmSessionRemote;
typedef struct _TlmSessionRemoteClass TlmSessionRemoteClass;
typedef struct _TlmSessionRemotePrivate TlmSessionRemotePrivate;

struct _TlmSessionRemote
{
    GObject parent;

    /* priv */
    TlmSessionRemotePrivate *priv;
};

struct _TlmSessionRemoteClass
{
    GObjectClass parent_class;
};

GType
tlm_session_remote_get_type (void) G_GNUC_CONST;

TlmSessionRemote *
tlm_session_remote_new (
        TlmConfig *config,
        const gchar *seat_id,
        const gchar *service,
        const gchar *username);

void
tlm_session_remote_create (
    TlmSessionRemote *session,
    const gchar *password,
    GHashTable *environment);

gboolean
tlm_session_remote_terminate (
        TlmSessionRemote *session);

G_END_DECLS

#endif /* __TLM_SESSION_REMOTE_H_ */
