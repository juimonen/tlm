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

#ifndef __TLM_DBUS_LOGIN_ADAPTER_H_
#define __TLM_DBUS_LOGIN_ADAPTER_H_

#include <config.h>
#include <glib.h>
#include "common/dbus/tlm-dbus-login-gen.h"
#include "tlm-dbus-utils.h"

G_BEGIN_DECLS

#define TLM_TYPE_LOGIN_ADAPTER                \
    (tlm_dbus_login_adapter_get_type())
#define TLM_DBUS_LOGIN_ADAPTER(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TLM_TYPE_LOGIN_ADAPTER, \
            TlmDbusLoginAdapter))
#define TLM_DBUS_LOGIN_ADAPTER_CLASS(klass)    \
    (G_TYPE_CHECK_CLASS_CAST((klass), TLM_TYPE_LOGIN_ADAPTER, \
            TlmDbusLoginAdapterClass))
#define TLM_IS_DBUS_LOGIN_ADAPTER(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TLM_TYPE_LOGIN_ADAPTER))
#define TLM_IS_DBUS_LOGIN_ADAPTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TLM_TYPE_LOGIN_ADAPTER))
#define TLM_DBUS_LOGIN_ADAPTER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), TLM_TYPE_LOGIN_ADAPTER, \
            TlmDbusLoginAdapterClass))

typedef struct _TlmDbusLoginAdapter TlmDbusLoginAdapter;
typedef struct _TlmDbusLoginAdapterClass TlmDbusLoginAdapterClass;
typedef struct _TlmDbusLoginAdapterPrivate TlmDbusLoginAdapterPrivate;

struct _TlmDbusLoginAdapter
{
    GObject parent;

    /* priv */
    TlmDbusLoginAdapterPrivate *priv;
};

struct _TlmDbusLoginAdapterClass
{
    GObjectClass parent_class;
};

GType tlm_dbus_login_adapter_get_type (void) G_GNUC_CONST;

TlmDbusLoginAdapter *
tlm_dbus_login_adapter_new_with_connection (
        GDBusConnection *connection);

void
tlm_dbus_login_adapter_request_completed (
        TlmDbusRequest *request,
        GError *error);

G_END_DECLS

#endif /* __TLM_DBUS_LOGIN_ADAPTER_H_ */
