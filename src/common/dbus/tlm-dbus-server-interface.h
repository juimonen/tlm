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

#ifndef __TLM_DBUS_SERVER_INTERFACE_H_
#define __TLM_DBUS_SERVER_INTERFACE_H_

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TLM_TYPE_DBUS_SERVER    (tlm_dbus_server_get_type ())
#define TLM_DBUS_SERVER(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        TLM_TYPE_DBUS_SERVER, TlmDbusServer))
#define TLM_IS_DBUS_SERVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        TLM_TYPE_DBUS_SERVER))
#define TLM_DBUS_SERVER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ( \
        (inst), TLM_TYPE_DBUS_SERVER, TlmDbusServerInterface))

typedef struct _TlmDbusServer TlmDbusServer; /* dummy object */
typedef struct _TlmDbusServerInterface TlmDbusServerInterface;

typedef enum {
    TLM_DBUS_SERVER_BUSTYPE_NONE = 0,
    TLM_DBUS_SERVER_BUSTYPE_MSG_BUS = 1,
    TLM_DBUS_SERVER_BUSTYPE_P2P = 2
} TlmDbusServerBusType;

struct _TlmDbusServerInterface {

    GTypeInterface parent;

    gboolean
    (*start) (TlmDbusServer *self);

    gboolean
    (*stop)  (TlmDbusServer *self);

    pid_t
    (*get_remote_pid)  (
            TlmDbusServer *self,
            GDBusMethodInvocation *invocation);
};

GType
tlm_dbus_server_get_type();

gboolean
tlm_dbus_server_start (
        TlmDbusServer *self);

gboolean
tlm_dbus_server_stop (
        TlmDbusServer *self);

pid_t
tlm_dbus_server_get_remote_pid (
        TlmDbusServer *self,
        GDBusMethodInvocation *invocation);

TlmDbusServerBusType
tlm_dbus_server_get_bus_type (
        TlmDbusServer *self);

G_END_DECLS

#endif /* __TLM_DBUS_SERVER_INTERFACE_H_ */
