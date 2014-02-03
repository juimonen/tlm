/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
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

#ifndef _TLM_DBUS_UTILS_H
#define _TLM_DBUS_UTILS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum
{
    TLM_DBUS_REQUEST_TYPE_LOGIN_USER,
    TLM_DBUS_REQUEST_TYPE_LOGOUT_USER,
    TLM_DBUS_REQUEST_TYPE_SWITCH_USER
} TlmDbusRequestType;

typedef struct
{
    TlmDbusRequestType type;
    GObject* dbus_adapter;
    GDBusMethodInvocation *invocation;
    gchar *seat_id;
    gchar *username;
    gchar *password;
    GHashTable *environment;
} TlmDbusRequest;

TlmDbusRequest *
tlm_dbus_utils_create_request (
        GObject *object,
        GDBusMethodInvocation *invocation,
        TlmDbusRequestType type,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment);

void
tlm_dbus_utils_dispose_request (
        TlmDbusRequest *request);

GVariant *
tlm_dbus_utils_hash_table_to_variant (GHashTable *dict);

GHashTable *
tlm_dbus_utils_hash_table_from_variant (GVariant *variant);

G_END_DECLS

#endif /* _TLM_DBUS_UTILS_H */
