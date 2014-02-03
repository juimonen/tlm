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

#ifndef __TLM_DBUS_SERVER_P2P_H_
#define __TLM_DBUS_SERVER_P2P_H_

#include <glib.h>
#include <glib-object.h>
#include <pwd.h>

G_BEGIN_DECLS

#define TLM_TYPE_DBUS_SERVER_P2P            (tlm_dbus_server_p2p_get_type())
#define TLM_DBUS_SERVER_P2P(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
        TLM_TYPE_DBUS_SERVER_P2P, TlmDbusServerP2P))
#define TLM_DBUS_SERVER_P2P_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
        TLM_TYPE_DBUS_SERVER_P2P, TlmDbusServerP2PClass))
#define TLM_IS_DBUS_SERVER_P2P(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
        TLM_TYPE_DBUS_SERVER_P2P))
#define TLM_IS_DBUS_SERVER_P2P_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
        TLM_TYPE_DBUS_SERVER_P2P))
#define TLM_DBUS_SERVER_P2P_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),\
        TLM_TYPE_DBUS_SERVER_P2P, TlmDbusServerP2PClass))

typedef struct _TlmDbusServerP2P TlmDbusServerP2P;
typedef struct _TlmDbusServerP2PClass TlmDbusServerP2PClass;
typedef struct _TlmDbusServerP2PPrivate TlmDbusServerP2PPrivate;

struct _TlmDbusServerP2P
{
    GObject parent;

    /* priv */
    TlmDbusServerP2PPrivate *priv;
};

struct _TlmDbusServerP2PClass
{
    GObjectClass parent_class;
};

GType
tlm_dbus_server_p2p_get_type();

TlmDbusServerP2P *
tlm_dbus_server_p2p_new (
        const gchar *address,
        uid_t uid);

#endif /* __TLM_DBUS_SERVER_P2P_H_ */
