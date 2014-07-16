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

#ifndef __TLM_SESSION_DAEMON_H_
#define __TLM_SESSION_DAEMON_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_SESSION_DAEMON  (tlm_session_daemon_get_type())
#define TLM_SESSION_DAEMON(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        TLM_TYPE_SESSION_DAEMON, TlmSessionDaemon))
#define TLM_SESSION_DAEMON_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), \
        TLM_TYPE_SESSION_DAEMON, TlmSessionDaemonClass))
#define TLM_IS_SESSION_DAEMON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        TLM_TYPE_SESSION_DAEMON))
#define TLM_IS_SESSION_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
        TLM_TYPE_SESSION_DAEMON))
#define TLM_SESSION_DAEMON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),\
        TLM_TYPE_SESSION_DAEMON, TlmSessionDaemonClass))

typedef struct _TlmSessionDaemon TlmSessionDaemon;
typedef struct _TlmSessionDaemonClass TlmSessionDaemonClass;
typedef struct _TlmSessionDaemonPrivate TlmSessionDaemonPrivate;

struct _TlmSessionDaemon
{
    GObject parent;

    /* priv */
    TlmSessionDaemonPrivate *priv;
};

struct _TlmSessionDaemonClass
{
    GObjectClass parent_class;
};

GType tlm_session_daemon_get_type();

TlmSessionDaemon *
tlm_session_daemon_new (
        gint in_fd,
        gint out_fd);

#endif /* __TLM_SESSION_DAEMON_H_ */
