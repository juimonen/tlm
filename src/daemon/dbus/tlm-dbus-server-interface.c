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

#include "tlm-dbus-server-interface.h"

G_DEFINE_INTERFACE (TlmDbusServer, tlm_dbus_server, 0);

static void
tlm_dbus_server_default_init (
        TlmDbusServerInterface *g_class)
{
    g_object_interface_install_property (g_class,  g_param_spec_uint ("bustype",
            "BusType",
            "Type of the Bus",
            0,
            G_MAXUINT16,
            TLM_DBUS_SERVER_BUSTYPE_NONE /* default value */,
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
            G_PARAM_STATIC_STRINGS));
}

gboolean
tlm_dbus_server_start (
        TlmDbusServer *self)
{
    g_return_val_if_fail (TLM_IS_DBUS_SERVER (self), FALSE);

    return TLM_DBUS_SERVER_GET_INTERFACE (self)->start (self);
}

gboolean
tlm_dbus_server_stop (
        TlmDbusServer *self)
{
    g_return_val_if_fail (TLM_IS_DBUS_SERVER (self), FALSE);

    return TLM_DBUS_SERVER_GET_INTERFACE (self)->stop (self);
}

pid_t
tlm_dbus_server_get_remote_pid (
        TlmDbusServer *self,
        GDBusMethodInvocation *invocation)
{
    g_return_val_if_fail (TLM_IS_DBUS_SERVER (self), FALSE);

    return TLM_DBUS_SERVER_GET_INTERFACE (self)->get_remote_pid (self,
            invocation);
}

TlmDbusServerBusType
tlm_dbus_server_get_bus_type (
        TlmDbusServer *self)
{
    guint16 bus_type;
    g_return_val_if_fail (TLM_IS_DBUS_SERVER (self),
            TLM_DBUS_SERVER_BUSTYPE_NONE);
    g_object_get (G_OBJECT (self), "bustype", &bus_type, NULL);
    return (TlmDbusServerBusType)bus_type;
}
